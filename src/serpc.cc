#include <string.h>
#include <errno.h>
#include <memory>
#include <thread>
#include <functional>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <serpc.h>

#define ENSURE(expr, fmt, ...) \
    do { \
        if (!(expr)) { \
            SERPC_LOG(ERROR, fmt, ##__VA_ARGS__); \
            return; \
        } \
    } while (0)

namespace serpc {

class UnixFd {
public:
    explicit UnixFd(int fd) noexcept: _fd(fd) { }
    ~UnixFd() noexcept {
        if (_fd != -1) {
            SERPC_LOG(DEBUG, "close %d", _fd);
            close(_fd);
        }
    }
    UnixFd(UnixFd&& rhs) noexcept: _fd(rhs._fd) {
        rhs._fd = -1;
    }
    UnixFd& operator=(UnixFd&& rhs) noexcept {
        _fd = rhs._fd;
        rhs._fd = -1;
        return *this;
    }
public:
    inline operator int() const noexcept { return _fd; }
private:
    int _fd;
};

class Defer {
public:
    explicit Defer(std::function<void()> f) noexcept: _f(f) { }
    ~Defer() { _f(); }
private:
    std::function<void()> _f;
};

Engine::Engine() noexcept {
    SERPC_LOG(DEBUG, "engine ctor");
}

Engine::~Engine() noexcept {
    SERPC_LOG(DEBUG, "engine dtor");
}

void Engine::run() noexcept {
    SERPC_LOG(DEBUG, "engine run");
    UnixFd kq { kqueue() };
    ENSURE(kq != -1, "kqueue fail: %s", strerror(errno));
    SERPC_LOG(DEBUG, "kqueue: %d", static_cast<int>(kq));
    struct addrinfo hint = { 0, PF_INET, SOCK_STREAM, IPPROTO_TCP };
    struct addrinfo* ai = nullptr;
    int rv = getaddrinfo("0.0.0.0", "12321", &hint, &ai);
    ENSURE(rv == 0, "getaddrinfo fail: %s", gai_strerror(rv));
    std::unique_ptr<struct addrinfo, void(*)(struct addrinfo*)> uai {
        ai, freeaddrinfo
    };
    UnixFd sock { socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol) };
    ENSURE(sock != -1, "socket fail: %s", strerror(errno));
    static const int reuseaddr = 1;
    rv = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
            &reuseaddr, sizeof(reuseaddr));
    ENSURE(rv == 0, "set reuseaddr fail: %s", strerror(errno));
    int flags = fcntl(sock, F_GETFL, 0);
    ENSURE(flags != -1, "fcntl getfl fail: %s", strerror(errno));
    rv = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    ENSURE(rv != -1, "fcntl setfl fail: %s", strerror(errno));
    rv = bind(sock, ai->ai_addr, ai->ai_addrlen);
    ENSURE(rv == 0, "bind fail: %s", strerror(errno));
    rv = listen(sock, 1024);
    ENSURE(rv == 0, "listen fail: %s", strerror(errno));
    std::thread client { [](struct addrinfo* ai) {
        UnixFd sock { socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol) };
        ENSURE(sock != -1, "client socket fail: %s", strerror(errno));
        int rv = connect(sock, ai->ai_addr, ai->ai_addrlen);
        ENSURE(rv == 0, "connect fail: %s", strerror(errno));
        SERPC_LOG(DEBUG, "client connected");
        ssize_t len = write(sock, "hello", 5);
        ENSURE(len == 5, "send fail: %s", strerror(errno));
    }, ai};
    Defer _ { [&client]() { client.join(); } };
    UnixFd ep { accept(sock, nullptr, nullptr) };
    ENSURE(ep != -1, "accept fail: %s", strerror(errno));
    char buf[5] = { '\0' };
    ssize_t len = read(ep, buf, sizeof(buf));
    ENSURE(len == sizeof(buf), "read fail: %s", strerror(errno));
    SERPC_LOG(DEBUG, "read %s", buf);
}

} /* namespace serpc */

