#ifndef SERPC_H
#define SERPC_H

#include <stdint.h>
#include <functional>

#define SERPC_LOG(level, fmt, ...) \
    ::serpc::log("SERPC " #level " %s %d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define SERPC_ENSURE(expr, fmt, ...) \
    do { \
        if (!(expr)) { \
            SERPC_LOG(ERROR, fmt, ##__VA_ARGS__); \
            return; \
        } \
    } while (0)

#define SERPC_ENSURE_OR(expr, val, fmt, ...) \
    do { \
        if (!(expr)) { \
            SERPC_LOG(ERROR, fmt, ##__VA_ARGS__); \
            return (val); \
        } \
    } while (0)

namespace serpc {

int log(const char* fmt, ...) noexcept;

struct Header {
    unsigned version:4;
    unsigned compress:4;
    unsigned reserved:24;
    uint32_t seq;
    uint16_t stub;
    uint16_t method;
    uint32_t size;
};

class ServerImpl;

class Server {
public:
    Server(const char* hostname, const char* servname);
    ~Server() noexcept;
    inline operator bool() noexcept { return _impl; }
private:
    ServerImpl* _impl;
};

class ClientImpl;

class Client {
public:
    Client(const char* hostname, const char* servname);
    ~Client() noexcept;
    inline operator bool() noexcept { return _impl; }
public:
    void invoke(std::function<void(int)>&& done) noexcept;
private:
    ClientImpl* _impl;
};

} /* namespace serpc */

#endif /* SERPC_H */

