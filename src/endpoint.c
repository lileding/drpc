#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <drpc.h>
#include "endpoint.h"

int drpc_listen(const char* hostname, const char* servname, int backlog) {
    static const struct addrinfo HINT = {
        0,
        PF_INET,
        SOCK_STREAM,
        IPPROTO_TCP,
        0,
        NULL,
        NULL,
        NULL,
    };
    struct addrinfo* ai = NULL;
    int rv = getaddrinfo(hostname, servname, &HINT, &ai);
    if (rv != 0) {
        DRPC_LOG(ERROR, "getaddrinfo fail: %s", gai_strerror(rv));
        return -1;
    }
    int sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock == -1) {
        DRPC_LOG(ERROR, "socket fail: %s", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }
    if (drpc_set_nonblock(sock)) {
        DRPC_LOG(ERROR, "set_nonblock fail: %s", strerror(errno));
        close(sock);
        freeaddrinfo(ai);
        return -1;
    }
    static const int REUSEADDR = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                &REUSEADDR, sizeof(REUSEADDR)) != 0) {
        DRPC_LOG(ERROR, "set reuseaddr fail: %s", strerror(errno));
        close(sock);
        freeaddrinfo(ai);
        return -1;
    }
    if (bind(sock, ai->ai_addr, ai->ai_addrlen) != 0) {
        DRPC_LOG(ERROR, "bind fail: %s", strerror(errno));
        close(sock);
        freeaddrinfo(ai);
        return -1;
    }
    freeaddrinfo(ai);
    if (listen(sock, backlog) != 0) {
        DRPC_LOG(ERROR, "listen fail: %s", strerror(errno));
        close(sock);
    }
    return sock;
}

int drpc_connect(const char* hostname, const char* servname) {
    static const struct addrinfo HINT = {
        0,
        PF_INET,
        SOCK_STREAM,
        IPPROTO_TCP,
        0,
        NULL,
        NULL,
        NULL,
    };
    struct addrinfo* ai = NULL;
    int rv = getaddrinfo(hostname, servname, &HINT, &ai);
    if (rv != 0) {
        DRPC_LOG(ERROR, "getaddrinfo fail: %s", gai_strerror(rv));
        return -1;
    }
    int sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock == -1) {
        DRPC_LOG(ERROR, "socket fail: %s", strerror(errno));
        freeaddrinfo(ai);
        return -1;
    }
    if (connect(sock, ai->ai_addr, ai->ai_addrlen) == -1) {
        DRPC_LOG(ERROR, "connect fail: %s", strerror(errno));
        close(sock);
        freeaddrinfo(ai);
        return -1;
    }
    if (drpc_set_nonblock(sock) == -1) {
        DRPC_LOG(ERROR, "set_nonblock fail: %s", strerror(errno));
        close(sock);
        return -1;
    }
    freeaddrinfo(ai);
    return sock;
}

int drpc_set_nonblock(int fd) {
    int rv = fcntl(fd, F_GETFL, 0);
    if (rv == -1) {
        DRPC_LOG(ERROR, "fcntl get fail: %s", strerror(errno));
        return -1;
    }
    return fcntl(fd, F_SETFL, rv | O_NONBLOCK);
}

