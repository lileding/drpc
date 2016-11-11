#include <string.h>
#include <errno.h>
#include <memory>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <serpc.h>
#include "io.h"
#include "utils.h"

namespace serpc {

static int set_nonblock(int fd) noexcept;
static void loop(int kq, int sock) noexcept;

Engine::Engine() noexcept {
    SERPC_LOG(DEBUG, "engine ctor");
}

Engine::~Engine() noexcept {
    SERPC_LOG(DEBUG, "engine dtor");
}

class Handler {
public:
    Handler() noexcept { }
    virtual ~Handler() noexcept { }
public:
    virtual void handle(const struct kevent* ev) noexcept = 0;
};

class Worker : public Handler {
public:
    explicit Worker(int kq) noexcept: _q(kq), _read(true) { }
public:
    void handle(const struct kevent* ev) noexcept override {
        if (ev->flags & EV_EOF) {
            delete reinterpret_cast<Worker*>(ev->udata);
            SERPC_LOG(DEBUG, "close client %lu", ev->ident);
            close(ev->ident);
            close(_kq);
            return;
        }
        if (_read) {
            on_read(ev);
        } else {
            on_write(ev);
        }
    }
private:
    void on_read(const struct kevent* ev) noexcept {
        SERPC_ENSURE(ev->filter & EVFILT_READ, "not read event");
        switch (_io->read(ev->ident)) {
        case CONTINUE:
            return;
        case ERROR:
            SERPC_LOG(WARNING, "read fail: %s", strerror(errno));
            close(ev->ident);
            return;
        case DONE:
            SERPC_LOG(DEBUG, "worker read %s", _buf);
            if (_q.change(ev->ident, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, this) == -1) {
                SERPC_LOG(WARNING, "kqueue add fail: %s", strerror(errno));
                close(ev->ident);
            }
            _read = false;
        }
    }
    void on_write(const struct kevent* ev) noexcept {
        SERPC_ENSURE(ev->filter & EVFILT_WRITE, "not write event");
        SERPC_LOG(DEBUG, "%lu writtable", ev->ident);
        ssize_t len = write(ev->ident, _buf, sizeof(_buf));
        if (len != sizeof(_buf)) {
            SERPC_LOG(WARNING, "write fail: %s", strerror(errno));
            close(ev->ident);
        }
        struct kevent nev;
        EV_SET(&nev, ev->ident, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, this);
        auto rv = kevent(_kq, &nev, 1, nullptr, 0, nullptr);
        if (rv == -1) {
            SERPC_LOG(WARNING, "kqueue add fail: %s", strerror(errno));
            close(ev->ident);
        }
        _read = true;
    }
private:
    Queue _q;
    bool _read;
    IOJob<6> _io;
};

class Acceptor : public Handler {
public:
    Acceptor(int sock, int kq) noexcept: _sock(sock), _kq(kq) { }
public:
    void handle(const struct kevent* ev) noexcept override {
        UnixFd ep { accept(_sock, nullptr, nullptr) };
        SERPC_ENSURE(ep != -1, "accept fail: %s", strerror(errno));
        auto rv = set_nonblock(ep);
        SERPC_ENSURE(rv != -1, "set_nonblock fail: %s", strerror(errno));
        struct kevent nev;
        EV_SET(&nev, ep, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, new Worker(_kq));
        rv = kevent(_kq, &nev, 1, nullptr, 0, nullptr);
        SERPC_ENSURE(rv != -1, "kevent add fail: %s", strerror(errno));
        ep.release();
    }
private:
    int _sock;
    int _kq;
};

void Engine::run() noexcept {
    SERPC_LOG(DEBUG, "engine run");
    UnixFd kq { kqueue() };
    SERPC_ENSURE(kq != -1, "kqueue fail: %s", strerror(errno));
    SERPC_LOG(DEBUG, "kqueue: %d", static_cast<int>(kq));
    struct addrinfo hint = { 0, PF_INET, SOCK_STREAM, IPPROTO_TCP };
    struct addrinfo* ai = nullptr;
    auto rv = getaddrinfo("0.0.0.0", "12321", &hint, &ai);
    SERPC_ENSURE(rv == 0, "getaddrinfo fail: %s", gai_strerror(rv));
    std::unique_ptr<struct addrinfo, void(*)(struct addrinfo*)> uai {
        ai, freeaddrinfo
    };
    UnixFd sock { socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol) };
    SERPC_ENSURE(sock != -1, "socket fail: %s", strerror(errno));
    static const int reuseaddr = 1;
    rv = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
            &reuseaddr, sizeof(reuseaddr));
    SERPC_ENSURE(rv == 0, "set reuseaddr fail: %s", strerror(errno));
    int flags = fcntl(sock, F_GETFL, 0);
    SERPC_ENSURE(flags != -1, "fcntl getfl fail: %s", strerror(errno));
    rv = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    SERPC_ENSURE(rv != -1, "fcntl setfl fail: %s", strerror(errno));
    rv = bind(sock, ai->ai_addr, ai->ai_addrlen);
    SERPC_ENSURE(rv == 0, "bind fail: %s", strerror(errno));
    rv = listen(sock, 1024);
    SERPC_ENSURE(rv == 0, "listen fail: %s", strerror(errno));
    std::thread client { [](struct addrinfo* ai) {
        UnixFd sock { socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol) };
        SERPC_ENSURE(sock != -1, "client socket fail: %s", strerror(errno));
        int rv = connect(sock, ai->ai_addr, ai->ai_addrlen);
        SERPC_ENSURE(rv == 0, "connect fail: %s", strerror(errno));
        SERPC_LOG(DEBUG, "client connected");
        char buf[] = "hello";
        ssize_t len = write(sock, buf, sizeof(buf));
        SERPC_ENSURE(len == sizeof(buf), "write fail: %s", strerror(errno));
        SERPC_LOG(DEBUG, "client write done");
        len = read(sock, buf, sizeof(buf));
        SERPC_ENSURE(len == sizeof(buf), "read fail: %s", strerror(errno));
        SERPC_LOG(DEBUG, "client got %s", buf);
        strcpy(buf, "olleh");
        len = write(sock, buf, sizeof(buf));
        SERPC_ENSURE(len == sizeof(buf), "write fail: %s", strerror(errno));
        SERPC_LOG(DEBUG, "client write done");
        len = read(sock, buf, sizeof(buf));
        SERPC_ENSURE(len == sizeof(buf), "read fail: %s", strerror(errno));
        SERPC_LOG(DEBUG, "client got %s", buf);
    }, ai };
    Defer _ { [&client]() { client.join(); } };
    struct kevent ev;
    EV_SET(&ev, sock, EVFILT_READ, EV_ADD, 0, 0, new Acceptor(sock, kq));
    SERPC_LOG(DEBUG, "set udata %p", ev.udata);
    rv = kevent(kq, &ev, 1, nullptr, 0, nullptr);
    SERPC_ENSURE(rv != -1, "kevent add fail: %s", strerror(errno));
    loop(kq, sock);
}

int set_nonblock(int fd) noexcept {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    } else {
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void loop(int kq, int sock) noexcept {
    struct kevent changes[100];
    for (int i = 0; i < 5; i++) {
        auto nchanges = kevent(kq, nullptr, 0,
                changes, sizeof(changes) / sizeof(struct kevent),
                nullptr);
        SERPC_ENSURE(nchanges != -1, "kevent wait fail: %s", strerror(errno));
        for (int i = 0; i < nchanges; i++) {
            auto& ev = changes[i];
            reinterpret_cast<Handler*>(ev.udata)->handle(&ev);
        }
    }
    SERPC_LOG(DEBUG, "no more loop");
}

} /* namespace serpc */

