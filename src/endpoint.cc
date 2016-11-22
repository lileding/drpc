#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <serpc.h>
#include "endpoint.h"

namespace serpc {

class AddrInfo {
public:
    AddrInfo(const char* hostname, const char* servname) noexcept;
    inline ~AddrInfo() noexcept { freeaddrinfo(_ai); }
public:
    inline operator struct addrinfo*() noexcept { return _ai; }
    inline operator const struct addrinfo*() const noexcept { return _ai; }
    inline struct addrinfo* operator->() noexcept { return _ai; }
private:
    struct addrinfo* _ai;
};

EndPoint EndPoint::listen(
        const char* hostname, const char* servname, int backlog) noexcept {
    AddrInfo ai { hostname, servname };
    if (!ai) return -1;
    auto sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    SERPC_ENSURE_OR(sock != -1, -1, "socket fail: %s", strerror(errno));
    EndPoint ep { sock };
    auto rv = fcntl(sock, F_GETFL, 0);
    SERPC_ENSURE_OR(rv != -1, -1, "fcntl get fail: %s", strerror(errno));
    rv = fcntl(sock, F_SETFL, rv | O_NONBLOCK);
    SERPC_ENSURE_OR(rv != -1, -1, "fcntl set fail: %s", strerror(errno));
    static const int REUSEADDR = 1;
    rv = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
            &REUSEADDR, sizeof(REUSEADDR));
    SERPC_ENSURE_OR(rv == 0, -1, "set reuseaddr fail: %s", strerror(errno));
    rv = bind(sock, ai->ai_addr, ai->ai_addrlen);
    SERPC_ENSURE_OR(rv == 0, -1, "bind fail: %s", strerror(errno));
    rv = ::listen(sock, backlog);
    SERPC_ENSURE_OR(rv == 0, -1, "listen fail: %s", strerror(errno));
    return ep;
}

EndPoint EndPoint::connect(
        const char* hostname, const char* servname) noexcept {
    AddrInfo ai { hostname, servname };
    if (!ai) return -1;
    auto sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    SERPC_ENSURE_OR(sock != -1, -1, "socket fail: %s", strerror(errno));
    EndPoint ep { sock };
    auto rv = ::connect(sock, ai->ai_addr, ai->ai_addrlen);
    SERPC_ENSURE_OR(rv != -1, -1, "connect fail: %s", strerror(errno));
    rv = fcntl(sock, F_GETFL, 0);
    SERPC_ENSURE_OR(rv != -1, -1, "fcntl get fail: %s", strerror(errno));
    rv = fcntl(sock, F_SETFL, rv | O_NONBLOCK);
    SERPC_ENSURE_OR(rv != -1, -1, "fcntl set fail: %s", strerror(errno));
    return ep;
}

EndPoint::~EndPoint() noexcept {
    if (_sock != -1) {
        close(_sock);
    }
}

int EndPoint::accept() noexcept {
    auto sock = ::accept(_sock, nullptr, nullptr);
    if (sock == -1) return -1;
    auto rv = fcntl(_sock, F_GETFL, 0);
    if (rv == -1) {
        SERPC_LOG(ERROR, "fcntl get fail: %s", strerror(errno));
        close(sock);
        return -1;
    }
    rv = fcntl(sock, F_SETFL, rv | O_NONBLOCK);
    if (rv == -1) {
        SERPC_LOG(ERROR, "fcntl set fail: %s", strerror(errno));
        close(sock);
        return -1;
    }
    return sock;
}

AddrInfo::AddrInfo(const char* hostname, const char* servname) noexcept:
        _ai(nullptr) {
    static const struct addrinfo HINT = {
        0, PF_INET, SOCK_STREAM, IPPROTO_TCP };
    auto rv = getaddrinfo(hostname, servname, &HINT, &_ai);
    SERPC_ENSURE(rv == 0, "getaddrinfo fail: %s", gai_strerror(rv));
}

} /* namespace serpc */

