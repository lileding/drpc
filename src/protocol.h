#ifndef DRPC_SRC_PROTOCOL_H
#define DRPC_SRC_PROTOCOL_H

#include <stdint.h>

namespace drpc {

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

} /* namespace drpc */

#endif /* DRPC_SRC_PROTOCOL_H */

