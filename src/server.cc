#include <functional>
#include <thread>
#include <unistd.h>
#include <serpc.h>
#include "endpoint.h"
#include "queue.h"
#include "io.h"

namespace serpc {

struct Session {
    Session(): is_header(1) { }
    IOJob<Header> header;
    IOJob<char> body;
    unsigned is_header:1;
};

class ServerImpl {
public:
    ServerImpl(const char* hostname, const char* servname);
    ~ServerImpl() noexcept;
    inline operator bool() noexcept {
        return _ep && _q && _thread.joinable();
    }
private:
    void loop() noexcept;
    void do_accept() noexcept;
    void do_read(int sock, Session* sess) noexcept;
    void do_write(int sock, Session* sess) noexcept;
private:
    bool do_read_header(int sock, Session* sess) noexcept;
    bool do_read_body(int sock, Session* sess) noexcept;
private:
    EndPoint _ep;
    Queue _q;
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
    if (_impl) {
        delete _impl;
        _impl = nullptr;
    }
}

ServerImpl::ServerImpl(const char* hostname, const char* servname):
        _ep(hostname, servname) {
    if (!_ep || !_q) {
        return;
    }
    if (!_ep.listen(1024)) {
        return;
    }
    auto rv = _q.change(_ep, EVFILT_READ, EV_ADD);
    SERPC_ENSURE(rv != -1, "add acceptor fail: %s", strerror(errno));
    _thread = std::thread { std::mem_fn(&ServerImpl::loop), this };
}

ServerImpl::~ServerImpl() noexcept {
    if (_thread.joinable()) {
        _thread.join();
    }
}

void ServerImpl::loop() noexcept {
    static const int NEVENT = 1024;
    struct kevent evs[NEVENT];
    while (1) {
        auto rv = kevent(static_cast<int>(_q), nullptr, 0, evs, NEVENT, nullptr);
        if (rv == -1) {
            SERPC_LOG(ERROR, "kevent fail: %s", strerror(errno));
            break;
        }
        for (int i = 0; i < rv; i++) {
            auto ev = &evs[i];
            if (ev->ident == static_cast<int>(_ep)) {
                do_accept();
            } else {
                if (ev->filter & EVFILT_READ) {
                    do_read(ev->ident, reinterpret_cast<Session*>(ev->udata));
                }
                if (ev->filter & EVFILT_WRITE) {
                    do_write(ev->ident, reinterpret_cast<Session*>(ev->udata));
                }
            }
        }
    }
}

void ServerImpl::do_accept() noexcept {
    while (1) {
        auto sock = accept(_ep, nullptr, nullptr);
        if (sock == -1) {
            if (errno == EAGAIN) {
                return;
            }
            break;
        }
        if (_q.change(sock, EVFILT_READ, EV_ADD | EV_CLEAR, new Session()) == -1) {
            SERPC_LOG(WARNING, "add kevent fail: %s", strerror(errno));
            shutdown(sock, SHUT_RDWR);
        }
    }
    // TODO close here
}

void ServerImpl::do_read(int sock, Session* sess) noexcept {
    bool goon = true;
    do {
        goon = sess->is_header ?
            do_read_header(sock, sess) :
            do_read_body(sock, sess);
    } while (goon);
}

void ServerImpl::do_write(int sock, Session* sess) noexcept {
}

bool ServerImpl::do_read_header(int sock, Session* sess) noexcept {
    auto rv = sess->header.read(sock);
    SERPC_LOG(DEBUG, "read header %d", rv);
    if (rv == CONTINUE) {
        return false;
    } else if (rv == ERROR) {
        close(sock);
        delete sess;
        return false;
    } else {
        auto h = sess->header;
        SERPC_LOG(DEBUG, "header, seq=%lu, size=%lu", h->seq, h->size);
        if (h->size == 0) {
            h.reset();
        } else {
            sess->body.reset(h->size);
            sess->is_header = false;
        }
        return true;
    }
}

bool ServerImpl::do_read_body(int sock, Session* sess) noexcept {
    auto rv = sess->body.read(sock);
    SERPC_LOG(DEBUG, "read body %d", rv);
    if (rv == CONTINUE) {
        return false;
    } else if (rv == ERROR) {
        close(sock);
        delete sess;
        return false;
    } else {
        SERPC_LOG(DEBUG, "read \"%s\"", sess->body.data());
        sess->is_header = true;
        sess->header.reset();
        return true;
    }
}

} /* namespace serpc */

