#ifndef DRPC_SRC_CHANNEL_H
#define DRPC_SRC_CHANNEL_H

#include <pthread.h>
#include <sys/uio.h>
#include <sys/queue.h>

struct drpc_item {
    STAILQ_ENTRY(drpc_item) entries;
    void* ptr;
};
typedef struct drpc_item* drpc_item_t;

struct drpc_channel {
    union {
        int fildes[2];
        struct {
            int receive;
            int send;
        };
    };
    STAILQ_HEAD(, drpc_item) items;
    pthread_mutex_t mutex;
};
typedef struct drpc_channel* drpc_channel_t;

int drpc_channel_open(drpc_channel_t chan);

void drpc_channel_close(drpc_channel_t chan);

void* drpc_channel_read(drpc_channel_t chan);

void drpc_channel_write(drpc_channel_t chan, void* ptr);

int drpc_channel_wait(drpc_channel_t chan);

#endif /* DRPC_SRC_CHANNEL_H */

