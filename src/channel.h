#ifndef DRPC_SRC_CHANNEL_H
#define DRPC_SRC_CHANNEL_H

#include <sys/uio.h>

struct drpc_channel {
    union {
        int fildes[2];
        struct {
            int receive;
            int send;
        };
    };
    void* ptr;
    struct iovec iov;
};
typedef struct drpc_channel* drpc_channel_t;

int drpc_channel_open(drpc_channel_t chan);

void drpc_channel_close(drpc_channel_t chan);

int drpc_channel_send(drpc_channel_t chan, void* ptr);

void* drpc_channel_recv(drpc_channel_t chan);

#endif /* DRPC_SRC_CHANNEL_H */

