#include <functional>
#include <thread>
#include <unistd.h>
#include <serpc.h>
#include "endpoint.h"
#include "queue.h"
#include "io.h"

namespace serpc {

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
    void do_read(int sock) noexcept;
    void do_write(int sock) noexcept;
private:
    bool do_read_header(int sock) noexcept;
    bool do_read_body(int sock) noexcept;
private:
    EndPoint _ep;
    Queue _q;
    std::thread _thread;
    bool _read_header;
    IOJob<Header> _header;
    IOJob<char> _body;
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
        _ep(hostname, servname), _read_header(true) {
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
                    do_read(ev->ident);
                }
                if (ev->filter & EVFILT_WRITE) {
                    do_write(ev->ident);
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
        if (_q.change(sock, EVFILT_READ, EV_ADD | EV_CLEAR) == -1) {
            SERPC_LOG(WARNING, "add kevent fail: %s", strerror(errno));
            shutdown(sock, SHUT_RDWR);
        }
    }
    // TODO close here
}

void ServerImpl::do_read(int sock) noexcept {
    bool goon = true;
    do {
        goon = _read_header ? do_read_header(sock) : do_read_body(sock);
    } while (goon);
}

void ServerImpl::do_write(int sock) noexcept {
}

bool ServerImpl::do_read_header(int sock) noexcept {
    auto rv = _header.read(sock);
    SERPC_LOG(DEBUG, "read header %d", rv);
    if (rv == CONTINUE) {
        return false;
    } else if (rv == ERROR) {
        close(sock);
        return false;
    } else {
        SERPC_LOG(DEBUG, "header, seq=%lu, size=%lu", _header->seq, _header->size);
        if (_header->size == 0) {
            _header.reset();
        } else {
            _body.reset(_header->size);
            _read_header = false;
        }
        return true;
    }
}

bool ServerImpl::do_read_body(int sock) noexcept {
    auto rv = _body.read(sock);
    SERPC_LOG(DEBUG, "read body %d", rv);
    if (rv == CONTINUE) {
        return false;
    } else if (rv == ERROR) {
        close(sock);
        return false;
    } else {
        SERPC_LOG(DEBUG, "read \"%s\"", _body.data());
        _read_header = true;
        _header.reset();
        return true;
    }
}

} /* namespace serpc */

