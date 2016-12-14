#ifndef DRPC_SRC_PROTOCOL_H
#define DRPC_SRC_PROTOCOL_H

#include <stdint.h>

struct drpc_header {
    unsigned version:4;
    unsigned compress:4;
    uint32_t sequence;
    uint32_t payload;
};

struct drpc_message {
    struct drpc_header header;
    char* body;
};

typedef struct drpc_message* drpc_message_t;

#endif /* DRPC_SRC_PROTOCOL_H */

