#ifndef DRPC_H
#define DRPC_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#define DRPC_LOGLEVEL_DEBUG     1
#define DRPC_LOGLEVEL_INFO      2
#define DRPC_LOGLEVEL_NOTICE    3
#define DRPC_LOGLEVEL_WARNING   4
#define DRPC_LOGLEVEL_ERROR     5
#define DRPC_LOGLEVEL_FATAL     6

#define DRPC_STATUS_OK          0

#define DRPC_LOG(level, fmt, ...) \
    do { \
        char __buf[27] = {'\0'}; \
        drpc_log(DRPC_LOGLEVEL_##level, "[DRPC] " #level " %s " __FILE__ ":%d " fmt "\n", \
             drpc_now(__buf, sizeof(__buf)), __LINE__, ##__VA_ARGS__); \
    } while (0)

#define DRPC_ENSURE(expr, fmt...) \
    do { \
        if (!(expr)) { \
            DRPC_LOG(ERROR, fmt); \
            return; \
        } \
    } while (0)

#define DRPC_ENSURE_OR(expr, val, fmt...) \
    do { \
        if (!(expr)) { \
            DRPC_LOG(ERROR, fmt); \
            return (val); \
        } \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

const char* drpc_now(char* buf, size_t bufsz);

int drpc_log(int level, const char* fmt, ...);

int drpc_set_loglevel(int level);

#if 0
/* CONTROLLER */
struct drpc_controller;
typedef struct drpc_controller* drpc_controller_t;

drpc_controller_t drpc_controller_new();

void drpc_controller_drop(drpc_controller_t controller);

void drpc_controller_wait(drpc_controller_t controller);
#endif

/* SERVER */
struct drpc_server;
typedef struct drpc_server* drpc_server_t;

drpc_server_t drpc_server_new(const char* hostname, const char* servname);

void drpc_server_join(drpc_server_t server);

#if 0
/* CLIENT */
struct drpc_client;
typedef struct drpc_client* drpc_client_t;

struct drpc_channel;
typedef struct drpc_channel* drpc_channel_t;

drpc_client_t drpc_client_new();

void drpc_client_drop(drpc_client_t client);

drpc_channel_t drpc_client_connect(
    drpc_client_t client, const char* hostname, const char* servname
);

void drpc_channel_call(
    drpc_channel_t chan, const char* request, size_t len,
    void (*callback)(const char*, size_t)
);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#ifdef __cplusplus
namespace drpc {

#if 0
class Controller {
public:
    inline Controller() noexcept:
        _controller(::drpc_controller_new()) { }
    inline ~Controller() noexcept {
        if (_controller) {
            ::drpc_controller_drop(_controller);
            _controller = nullptr;
        }
    }
    inline operator ::drpc_controller_t() noexcept {
        return _controller;
    }
    inline operator bool() noexcept {
        return _controller != nullptr;
    }
    inline Controller(Controller&& rhs) noexcept:
            _controller(rhs._controller) {
        rhs._controller = nullptr;
    }
    inline Controller& operator=(Controller&& rhs) noexcept {
        _controller = rhs._controller;
        rhs._controller = nullptr;
        return *this;
    }
public:
    inline void wait() noexcept {
        return ::drpc_controller_wait(_controller);
    }
private:
    ::drpc_controller_t _controller;
};
#endif

class Server {
public:
    inline Server(const char* hostname, const char* servname) noexcept:
        _server(::drpc_server_new(hostname, servname)) { }
    inline ~Server() noexcept {
        if (_server) {
            ::drpc_server_drop(_server);
        }
    }
    inline operator ::drpc_server_t() noexcept {
        return _server;
    }
    inline operator bool() noexcept {
        return _server != nullptr;
    }
    inline Server(Server&& rhs) noexcept: _server(rhs._server) {
        rhs._server = nullptr;
    }
    inline Server& operator=(Server&& rhs) noexcept {
        _server = rhs._server;
        rhs._server = nullptr;
        return *this;
    }
private:
    ::drpc_server_t _server;
};

#if 0
class String {
public:
    inline String() noexcept {
        _iov.iov_base = nullptr;
        _iov.iov_len = 0;
    }
    inline explicit String(const char* str) noexcept {
        _iov.iov_len = strlen(str);
        _iov.iov_base = strndup(str, _iov.iov_len);
    }
    inline explicit String(const char* str, size_t len) noexcept {
        _iov.iov_len = len;
        _iov.iov_base = strndup(str, _iov.iov_len);
    }
    inline ~String() noexcept {
        if (_iov.iov_base != nullptr) {
            free(_iov.iov_base);
            _iov.iov_len = 0;
        }
    }
    inline String(const String& rhs) noexcept {
        _iov.iov_base = strndup((char*)rhs._iov.iov_base, rhs._iov.iov_len);
        _iov.iov_len = rhs._iov.iov_len;
    }
    inline String(String&& rhs) noexcept: _iov(rhs._iov) {
        rhs._iov.iov_base = nullptr;
        rhs._iov.iov_len = 0;
    }
    inline String& operator=(const String& rhs) noexcept {
        _iov.iov_base = strndup((char*)rhs._iov.iov_base, rhs._iov.iov_len);
        _iov.iov_len = rhs._iov.iov_len;
        return *this;
    }
    inline String& operator=(String&& rhs) noexcept {
        _iov.iov_base = rhs._iov.iov_base;
        _iov.iov_len = rhs._iov.iov_len;
        rhs._iov.iov_base = nullptr;
        rhs._iov.iov_len = 0;
        return *this;
    }
public:
    inline char& operator[](size_t index) noexcept {
        return ((char*)_iov.iov_base)[index];
    }
    inline char operator[](size_t index) const noexcept {
        return ((char*)_iov.iov_base)[index];
    }
public:
    inline operator struct iovec*() noexcept { return &_iov; }
    inline operator const struct iovec*() const noexcept { return &_iov; }
    inline operator struct iovec&() noexcept { return _iov; }
    inline operator const struct iovec&() const noexcept { return _iov; }
    inline struct iovec* operator->() noexcept { return &_iov; }
    inline const struct iovec* operator->() const noexcept { return &_iov; }
    inline const char* c_str() const noexcept { return (const char*)_iov.iov_base; }
    inline const char* data() const noexcept { return (const char*)_iov.iov_base; }
    inline size_t size() const noexcept { return _iov.iov_len; }
    inline size_t length() const noexcept { return _iov.iov_len; }
    inline size_t capacity() const noexcept { return _iov.iov_len; }
    inline bool empty() const noexcept { return _iov.iov_len == 0; }
private:
    struct iovec _iov;
};
#endif

class Client {
public:
    inline Client() noexcept:
        _client(::drpc_client_new()) { }
    inline ~Client() noexcept {
        if (_client) {
            ::drpc_client_drop(_client);
        }
    }
    inline operator ::drpc_client_t() noexcept {
        return _client;
    }
    inline operator bool() noexcept {
        return _client != nullptr;
    }
    inline Client(Client&& rhs) noexcept: _client(rhs._client) {
        rhs._client = nullptr;
    }
    inline Client& operator=(Client&& rhs) noexcept {
        _client = rhs._client;
        rhs._client = nullptr;
        return *this;
    }
public:
    ::drpc_channel_t connect(const char* hostname, const char* servname) noexcept {
        return drpc_client_connect(_client, hostname, servname);
    }
private:
    ::drpc_client_t _client;
};

} /* namespace drpc */
#endif /* __cplusplus */

#endif /* DRPC_H */

