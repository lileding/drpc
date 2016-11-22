#include <thread>
#include <map>
#include <queue>
#include <drpc.h>
#include "signal.h"
#include "endpoint.h"
#include "queue.h"
#include "channel.h"
#include "controller.h"

namespace drpc {

class ClientImpl;

class ClientChannel : public Channel {
public:
    inline ClientChannel(ClientImpl& client, int sock): Channel(sock), _client(client) { }
public:
    bool on_message(const Message& msg) noexcept;
private:
    ClientImpl& _client;
};

class ClientImpl {
public:
    friend class ClientChannel;
    inline ClientImpl(const char* hostname, const char* servname) noexcept;
    ~ClientImpl() noexcept;
    inline operator bool() noexcept {
        return _stop && _ep && _q && _thread.joinable();
    }
public:
    void call(Controller* cntl,
            const std::string& request, std::string* response) noexcept;
private:
    void loop() noexcept;
private:
    Signal _stop;
    EndPoint _ep;
    ClientChannel _chan;
    Queue _q;
    std::thread _thread;
    uint32_t _seq;
    std::map<uint32_t, Controller*> _controllers;
};

Client::Client(const char* hostname, const char* servname):
        _impl(new ClientImpl(hostname, servname)) {
    if (!*_impl) {
        delete _impl;
        _impl = nullptr;
    }
}

Client::~Client() noexcept {
    if (!*this) return;
    DRPC_LOG(DEBUG, "client dtor");
    delete _impl;
    _impl = nullptr;
}

void Client::call(Controller* cntl,
        const std::string& request, std::string* response) noexcept {
    return _impl->call(cntl, request, response);
}

bool ClientChannel::on_message(const Message& msg) noexcept {
    ControllerImpl* cc = *_client._controllers[msg.header.sequence];
    cc->buffer()->assign(reinterpret_cast<char*>(msg.body), msg.header.payload);
    _client._controllers.erase(msg.header.sequence);
    cc->unlock();
    return true;
}

ClientImpl::ClientImpl(const char* hostname, const char* servname) noexcept:
        _ep(EndPoint::connect(hostname, servname)), _chan(*this, _ep), _seq(0) {
    if (!_stop || !_ep || !_q) {
        return;
    }
    auto rv = _q.change(_stop.receiver(), EVFILT_READ, EV_ADD | EV_ONESHOT);
    DRPC_ENSURE(rv != -1, "add stop receiver fail: %s", strerror(errno));
    rv = _q.change(_chan.ident(), EVFILT_READ | EVFILT_WRITE, EV_ADD | EV_CLEAR);
    DRPC_ENSURE(rv != -1, "add channel fail: %s", strerror(errno));
    _thread = std::thread { std::mem_fn(&ClientImpl::loop), this };
}

ClientImpl::~ClientImpl() noexcept {
    if (!*this) return;
    _stop.notify();
    _thread.join();
}

void ClientImpl::call(Controller* cntl,
        const std::string& request, std::string* response) noexcept {
    auto cc = static_cast<ControllerImpl*>(*cntl);
    auto seq = _seq++;
    _controllers[seq] = cntl;
    char* body = strndup(request.data(), request.size());
    Message msg {
        Header {
            0x1,
            0x0,
            seq,
            static_cast<uint32_t>(request.size()),
        },
        body,
    };
    cc->lock(response);
    _chan.send(msg);
}

void ClientImpl::loop() noexcept {
    struct kevent evs[2];
    bool stop = false;
    do {
        auto rv = kevent(_q, nullptr, 0, evs, 2, nullptr);
        if (rv == -1) {
            DRPC_LOG(WARNING, "kevent fail: %s", strerror(errno));
            break;
        }
        for (int i = 0; i < rv; i++) {
            auto ev = &evs[i];
            if (ev->ident == _stop.receiver()) {
                DRPC_LOG(DEBUG, "client got stop event");
                stop = true;
                break;
            } else {
                _chan.on_event(&_q, ev);
            }
        }
    } while (!stop);
}

} /* namespace drpc */

