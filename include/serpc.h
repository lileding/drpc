#ifndef SERPC_H
#define SERPC_H

#include <string>
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

class ServerImpl;

class Server {
public:
    Server(const char* hostname, const char* servname);
    ~Server() noexcept;
    inline operator bool() noexcept { return _impl; }
    inline Server(Server&& rhs) noexcept: _impl(rhs._impl) {
        rhs._impl = nullptr;
    }
    inline Server& operator=(Server&& rhs) noexcept {
        _impl = rhs._impl;
        rhs._impl = nullptr;
        return *this;
    }
private:
    ServerImpl* _impl;
};

class ControllerImpl;

class Controller {
public:
    Controller() noexcept;
    ~Controller() noexcept;
    inline operator bool() noexcept { return _impl != nullptr; }
    inline Controller(Controller&& rhs) noexcept: _impl(rhs._impl) {
        rhs._impl = nullptr;
    }
    inline Controller& operator=(Controller&& rhs) noexcept {
        _impl = rhs._impl;
        rhs._impl = nullptr;
        return *this;
    }
    inline operator ControllerImpl*() noexcept { return _impl; }
public:
    void wait() noexcept;
private:
    ControllerImpl* _impl;
};

class ClientImpl;

class Client {
public:
    Client(const char* hostname, const char* servname);
    ~Client() noexcept;
    inline operator bool() noexcept { return _impl; }
    inline Client(Client&& rhs) noexcept: _impl(rhs._impl) {
        rhs._impl = nullptr;
    }
    inline Client& operator=(Client&& rhs) noexcept {
        _impl = rhs._impl;
        rhs._impl = nullptr;
        return *this;
    }
public:
    void call(Controller* cntl,
            const std::string& request, std::string* response
    ) noexcept;
private:
    ClientImpl* _impl;
};

} /* namespace serpc */

#endif /* SERPC_H */

