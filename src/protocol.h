#ifndef DRPC_SRC_PROTOCOL_H
#define DRPC_SRC_PROTOCOL_H

#include <stdint.h>

struct drpc_request_header {
    unsigned version:4;
    unsigned compress:4;
    uint8_t padding;
    uint16_t method;
    uint32_t sequence;
    uint32_t payload;
};

struct drpc_response_header {
    unsigned version:4;
    unsigned compress:4;
    uint8_t padding;
    uint16_t status;
    uint32_t sequence;
    uint32_t payload;
};

struct drpc_request {
    struct drpc_request_header header;
    char* body;
};
typedef struct drpc_request* drpc_request_t;

struct drpc_response {
    struct drpc_response_header header;
    char* body;
};
typedef struct drpc_response* drpc_response_t;

#endif /* DRPC_SRC_PROTOCOL_H */

