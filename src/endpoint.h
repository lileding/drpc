#ifndef SERPC_SRC_ENDPOINT_H
#define SERPC_SRC_ENDPOINT_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

namespace serpc {

class EndPoint {
public:
    EndPoint(const char* hostname, const char* servname) noexcept;
    ~EndPoint() noexcept;
    inline operator bool() noexcept { return _sock != -1; }
public:
    inline operator int() noexcept { return _sock; }
    inline operator intptr_t() noexcept { return _sock; }
    bool listen(int backlog) noexcept;
    bool connect() noexcept;
private:
    int _sock;
    struct addrinfo* _ai;
};

} /* namespace serpc */

#endif /* SERPC_SRC_ENDPOINT_H */

