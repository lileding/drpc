#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "server.h"
#include "endpoint.h"
#include "channel.h"

static void* do_loop(void* arg);
static void on_accept(drpc_server_t server);

drpc_server_t drpc_server_new(const char* hostname, const char* servname) {
    DRPC_ENSURE_OR(hostname && servname, NULL, "invalid argument");
    drpc_server_t server = (drpc_server_t)malloc(sizeof(*server));
    if (!server) {
        return NULL;
    }
    server->endpoint = drpc_listen(hostname, servname, 1024);
    if (server->endpoint == -1) {
        drpc_server_drop(server);
        return NULL;
    }
    /* FIXME if (drpc_signal_open(&server->quit) != 0) {
        drpc_server_drop(server);
        return NULL;
    }*/
    if (drpc_queue_open(&server->queue) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    if (pthread_create(&server->thread, NULL, do_loop, server) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    if (drpc_queue_add(&server->queue, server->endpoint,
                DRPC_FILT_READ, NULL) != 0) {
        drpc_server_drop(server);
        return NULL;
    }
    return server;
}

void drpc_server_drop(drpc_server_t server) {
    if (!server) {
        return;
    }
    drpc_signal_notify(&server->quit);
    pthread_join(server->thread, NULL); /* wait for end loop */
    // FIXME drpc_signal_close(&server->quit);
    drpc_queue_close(&server->queue);
    if (server->endpoint != -1) {
        close(server->endpoint);
    }
    free(server);
    DRPC_LOG(DEBUG, "server exit");
}

void* do_loop(void* arg) {
    drpc_server_t server = (drpc_server_t)arg;
    DRPC_LOG(DEBUG, "server works, endpoint=%d, queue=%d", server->endpoint, server->queue.kq);
    while (1) {
        static const size_t NEVENTS = 1024;
        struct kevent evs[NEVENTS];
        int rv = kevent(server->queue.kq, NULL, 0, evs, NEVENTS, NULL);
        if (rv < 0) {
            DRPC_LOG(ERROR, "queue fail: %s", strerror(errno));
            break;
        }
        for (int i = 0; i < rv; i++) {
            const struct kevent *ev = &evs[i];
            if (ev->ident == server->endpoint) {
                on_accept(server);
            } else {
                drpc_channel_t chan = (drpc_channel_t)ev->udata;
                if (drpc_channel_process(chan, ev->filter) == -1) {
                    drpc_channel_drop(chan);
                }
            }
        }
    }
    return NULL;
}

void on_accept(drpc_server_t server) {
    int sock = -1;
    while (1) {
        sock = accept(server->endpoint, NULL, NULL);
        if (sock == -1) {
            if (errno != EAGAIN) {
                DRPC_LOG(ERROR, "accept fail: %s", strerror(errno));
            }
            break;
        }
        drpc_channel_t chan = drpc_channel_new(sock);
        if (!chan) {
            close(sock);
            continue;
        }
        if (drpc_queue_add(&server->queue, sock,
                    DRPC_FILT_READ | DRPC_FILT_WRITE | DRPC_EVENT_EDGE,
                    chan) != 0) {
            drpc_channel_drop(chan);
            continue;
        }
        DRPC_LOG(DEBUG, "server accept sock=%d", sock);
    }
}

#if 0
#include <stdlib.h>
#include <memory>
#include <functional>
#include <thread>
#include <unistd.h>
#include <drpc.h>
#include "signal.h"
#include "endpoint.h"
#include "queue.h"
#include "channel.h"

namespace drpc {

class ServerChannel : public Channel {
public:
    inline explicit ServerChannel(int sock) noexcept: Channel(sock) { }
protected:
    bool on_message(const Message& msg) noexcept;
};

class ServerImpl {
public:
    ServerImpl(const char* hostname, const char* servname);
    ~ServerImpl() noexcept;
    inline operator bool() noexcept {
        return _ep && _q && _stop && _thread.joinable();
    }
private:
    void loop() noexcept;
    void do_accept() noexcept;
private:
    EndPoint _ep;
    Queue _q;
    Signal _stop;
    std::thread _thread;
};

Server::Server(const char* hostname, const char* servname):
        _impl(new ServerImpl(hostname, servname)) {
    if (!*_impl) {
        delete _impl;
        _impl = nullptr;
    }
}

Server::~Server() noexcept {
    if (!*this) return;
    delete _impl;
    _impl = nullptr;
}

ServerImpl::ServerImpl(const char* hostname, const char* servname):
        _ep(EndPoint::listen(hostname, servname)) {
    if (!_ep || !_q || !_stop) return;
    auto rv = _q.change(_stop.receiver(), EVFILT_READ, EV_ADD | EV_ONESHOT);
    DRPC_ENSURE(rv != -1, "add stop receiver fail: %s", strerror(errno));
    rv = _q.change(_ep, EVFILT_READ, EV_ADD);
    DRPC_ENSURE(rv != -1, "add acceptor fail: %s", strerror(errno));
    _thread = std::thread { std::mem_fn(&ServerImpl::loop), this };
}

ServerImpl::~ServerImpl() noexcept {
    if (!*this) return;
    _stop.notify();
    _thread.join();
}

void ServerImpl::loop() noexcept {
    static const int NEVENT = 1024;
    struct kevent evs[NEVENT];
    bool stop = false;
    do {
        auto rv = kevent(_q, nullptr, 0, evs, NEVENT, nullptr);
        if (rv == -1) {
            break;
        }
        for (int i = 0; i < rv; i++) {
            auto ev = &evs[i];
            if (ev->ident == static_cast<int>(_ep)) {
                do_accept();
            } else if (ev->ident == _stop.receiver()) {
                DRPC_LOG(DEBUG, "server got STOP event");
                stop = true;
                break;
            } else {
                reinterpret_cast<Channel*>(ev->udata)->on_event(&_q, ev);
            }
        }
    } while (!stop);

    DRPC_LOG(ERROR, "kevent fail: %s", strerror(errno));
    // TODO release channel here
}

void ServerImpl::do_accept() noexcept {
    while (1) {
        auto sock = _ep.accept();
        if (sock == -1) {
            if (errno != EAGAIN) {
                DRPC_LOG(ERROR, "accept fail: %s", strerror(errno));
            }
            break;
        }
        std::unique_ptr<Channel> chan { new ServerChannel(sock) };
        auto rv = _q.change(
                sock, EVFILT_READ | EVFILT_WRITE, EV_ADD | EV_CLEAR,
                chan.get());
        if (rv == -1) {
            DRPC_LOG(WARNING, "add kevent fail: %s", strerror(errno));
            continue;
        }
        chan.release();
    }
}

bool ServerChannel::on_message(const Message& msg) noexcept {
    char* body = strndup(reinterpret_cast<char*>(msg.body), msg.header.payload);
    Message reply {
        Header {
            0x1,
            0x0,
            msg.header.sequence,
            6
        },
        body,
    };
    send(reply);
    return true;
}

} /* namespace drpc */
#endif

