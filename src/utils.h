#ifndef SERPC_SRC_UTILS_H
#define SERPC_SRC_UTILS_H

#include <unistd.h>
#include <functional>
#include <serpc.h>

#define SERPC_ENSURE(expr, fmt, ...) \
    do { \
        if (!(expr)) { \
            SERPC_LOG(ERROR, fmt, ##__VA_ARGS__); \
            return; \
        } \
    } while (0)

namespace serpc {

class UnixFd {
public:
    explicit inline UnixFd(int fd) noexcept: _fd(fd) { }
    inline ~UnixFd() noexcept {
        if (_fd != -1) {
            SERPC_LOG(DEBUG, "close %d", _fd);
            close(_fd);
        }
    }
    inline UnixFd(UnixFd&& rhs) noexcept: _fd(rhs._fd) {
        rhs._fd = -1;
    }
    inline UnixFd& operator=(UnixFd&& rhs) noexcept {
        _fd = rhs._fd;
        rhs._fd = -1;
        return *this;
    }
public:
    inline int release() noexcept {
        int fd = _fd;
        _fd = -1;
        return fd;
    }
    inline operator int() const noexcept { return _fd; }
private:
    int _fd;
};

class Defer {
public:
    explicit inline Defer(std::function<void()> f) noexcept: _f(f) { }
    inline ~Defer() { _f(); }
private:
    std::function<void()> _f;
};

} /* namespace serpc */

#endif /* SERPC_SRC_UTILS_H */

