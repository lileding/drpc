#ifndef SERPC_SRC_ENDPOINT_H
#define SERPC_SRC_ENDPOINT_H

#include <stdint.h>

namespace serpc {

class EndPoint {
public:
    static EndPoint listen(
            const char* hostname, const char* servname, int backlog=1024) noexcept;
    static EndPoint connect(
            const char* hostname, const char* servname) noexcept;
    ~EndPoint() noexcept;
    inline operator bool() noexcept { return _sock != -1; }
    inline EndPoint(EndPoint&& rhs) noexcept: _sock(rhs._sock) { rhs._sock = -1; }
    inline EndPoint& operator=(EndPoint&& rhs) noexcept {
        _sock = rhs._sock;
        rhs._sock = -1;
        return *this;
    }
public:
    int accept() noexcept;
    inline operator int() noexcept { return _sock; }
    inline operator intptr_t() noexcept { return _sock; }
private:
    inline EndPoint(int sock) noexcept: _sock(sock) { }
private:
    int _sock;
};

} /* namespace serpc */

#endif /* SERPC_SRC_ENDPOINT_H */

