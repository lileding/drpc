#ifndef SERPC_SRC_PROTOCOL_H
#define SERPC_SRC_PROTOCOL_H

#include <stdint.h>

namespace serpc {

struct Header {
    unsigned version:4;
    unsigned compress:4;
    uint32_t sequence;
    uint32_t payload;
};

struct Message {
    struct Header header;
    void* body;
};

} /* namespace serpc */

#endif /* SERPC_SRC_PROTOCOL_H */

