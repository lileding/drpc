#ifndef SERPC_SRC_CLOSURE_H
#define SERPC_SRC_CLOSURE_H

#if 0
#include "protocol.h"

namespace serpc {

struct Header;

class Channel;

class Closure {
public:
    inline explicit Closure(Channel* chan) noexcept:
        _chan(chan), _request_body(nullptr), _response_body(nullptr) { }
    ~Closure() noexcept;
public:
    void call(const struct RequestHeader* header, const char* body) noexcept;
    inline const ResponseHeader* response_header() noexcept { return &_response_header; }
    inline const char* response_body() noexcept { return _response_body; }
private:
    Channel* _chan;
    const char* _request_body;
    ResponseHeader _response_header;
    char* _response_body;
};

/**
    void method(const ::serpc::Controller& cntl,
                const std::string& request,
                std::string* response) noexcept {
        response->assign(buf, bufsz);
        cntl->done();
        cntl->fail(123);
    }
**/

} /* namespace serpc */
#endif

#endif /* SERPC_SRC_CLOSURE_H */

