#include <thread>
#include <queue>
#include <serpc.h>
#include "signal.h"
#include "endpoint.h"
#include "queue.h"
#include "io.h"

namespace serpc {

struct Message {
    struct Header header;
    char* body;
};

class ClientImpl {
public:
    inline ClientImpl(const char* hostname, const char* servname) noexcept;
    ~ClientImpl() noexcept;
    inline operator bool() noexcept {
        return _stop && _ep && _q && _thread.joinable();
    }
public:
    void invoke(std::function<void(int)>&& done) noexcept;
private:
    void do_read() noexcept;
    void do_write() noexcept;
    void loop() noexcept;
private:
    Signal _stop;
    EndPoint _ep;
    Queue _q;
    std::thread _thread;
    uint32_t _seq;
    std::queue<Message> _pbuf;
};

Client::Client(const char* hostname, const char* servname):
        _impl(new ClientImpl(hostname, servname)) {
    if (!*_impl) {
        delete _impl;
        _impl = nullptr;
    }
}

Client::~Client() noexcept {
    if (_impl) {
        delete _impl;
        _impl = nullptr;
    }
}

void Client::invoke(std::function<void(int)>&& done) noexcept {
    return _impl->invoke(std::move(done));
}

ClientImpl::ClientImpl(const char* hostname, const char* servname) noexcept:
        _ep(hostname, servname), _seq(0) {
    if (!_stop || !_ep || !_q) {
        return;
    }
    auto rv = _q.change(_stop.receiver(), EVFILT_READ, EV_ADD | EV_ONESHOT);
    SERPC_ENSURE(rv != -1, "add stop receiver fail: %s", strerror(errno));
    SERPC_ENSURE(_ep.connect(), "connect fail");
    //rv = _q.change(_ep, EVFILT_READ | EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, this);
    //SERPC_ENSURE(rv != -1, "add worker fail: %s", strerror(errno));
    _thread = std::thread { std::mem_fn(&ClientImpl::loop), this };
}

ClientImpl::~ClientImpl() noexcept {
    if (_thread.joinable()) {
        _thread.join();
    }
}

void ClientImpl::invoke(std::function<void(int)>&& done) noexcept {
    IOJob<Header> header;
    header->version = 0x1;
    header->compress = 0x0;
    header->seq = _seq++;
    header->size = 0;
    auto rv = header.write(_ep);
    SERPC_LOG(DEBUG, "client write %d", rv);
    //_pbuf.push(std::move(msg));
    // _callbacks.push(std::move(done));
    //do_write();
}

void ClientImpl::loop() noexcept {
    struct kevent evs[2];
    int rv = -1;
    do {
        rv = kevent(_q, nullptr, 0, evs, 2, nullptr);
        for (int i = 0; i < rv; i++) {
            auto ev = &evs[i];
            if (ev->ident == _stop.receiver()) {
                return;
            } else {
                if (ev->filter & EVFILT_READ) {
                    do_read();
                }
                if (ev->filter & EVFILT_WRITE) {
                    do_write();
                }
            }
        }
    } while (rv != -1);
}

void ClientImpl::do_read() noexcept {
}

void ClientImpl::do_write() noexcept {
}

} /* namespace serpc */

