#if 0
#include <stdlib.h>
#include <serpc.h>
#include "closure.h"
#include "channel.h"

namespace serpc {

Closure::~Closure() noexcept {
    SERPC_LOG(DEBUG, "closure dtor");
    if (_request_body) {
        free(const_cast<char*>(_request_body));
        _request_body = nullptr;
    }
    if (_response_body) {
        free(_response_body);
        _response_body = nullptr;
    }
}

void Closure::call(const struct RequestHeader* header, const char* body) noexcept {
    SERPC_LOG(DEBUG, "RPC request:%s", body);
    _response_header.version = header->version;
    _response_header.compress = header->compress;
    _response_header.sequence = header->sequence;
    _response_header.payload = header->payload;
    _response_header.rcode = 0;
    _response_body = strndup(body, _response_header.payload);
    _chan->response(this);
}

} /* namespace serpc */
#endif

