#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <serpc.h>
#include "endpoint.h"
#include "utils.h"

namespace serpc {

EndPoint::EndPoint(const char* hostname, const char* servname) noexcept:
        _sock(-1), _ai(nullptr) {
    static const struct addrinfo HINT = {
        0, PF_INET, SOCK_STREAM, IPPROTO_TCP };
    auto rv = getaddrinfo(hostname, servname, &HINT, &_ai);
    SERPC_ENSURE(rv == 0, "getaddrinfo fail: %s", gai_strerror(rv));
    _sock = socket(_ai->ai_family, _ai->ai_socktype, _ai->ai_protocol);
    SERPC_ENSURE(_sock != -1, "socket fail: %s", strerror(errno));
}

EndPoint::~EndPoint() noexcept {
    if (_ai) {
        freeaddrinfo(_ai);
        _ai = nullptr;
    }
    if (_sock) {
        close(_sock);
        _sock = -1;
    }
}

bool EndPoint::listen(int backlog) noexcept {
    if (!*this) {
        return false;
    }
    static const int REUSEADDR = 1;
    auto rv = setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR,
            &REUSEADDR, sizeof(REUSEADDR));
    SERPC_ENSURE_OR(rv == 0, false, "set reuseaddr fail: %s", strerror(errno));
    rv = set_nonblock(_sock);
    SERPC_ENSURE_OR(rv != -1, false, "set_nonblock fail: %s", strerror(errno));
    rv = bind(_sock, _ai->ai_addr, _ai->ai_addrlen);
    SERPC_ENSURE_OR(rv == 0, false, "bind fail: %s", strerror(errno));
    freeaddrinfo(_ai);
    _ai = nullptr;
    rv = ::listen(_sock, backlog);
    SERPC_ENSURE_OR(rv == 0, -1, "listen fail: %s", strerror(errno));
    return true;
}

bool EndPoint::connect() noexcept {
    if (!*this) {
        return false;
    }
    auto rv = ::connect(_sock, _ai->ai_addr, _ai->ai_addrlen);
    SERPC_ENSURE_OR(rv != -1, false, "connect fail: %s", strerror(errno));
    freeaddrinfo(_ai);
    _ai = nullptr;
    return true;
}

} /* namespace serpc */

