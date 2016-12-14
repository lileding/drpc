#ifndef DRPC_SRC_QUEUE_H
#define DRPC_SRC_QUEUE_H

#include <stdint.h>
#include <sys/event.h>

struct drpc_queue {
    int kq;
};
typedef struct drpc_queue* drpc_queue_t;

int drpc_queue_open(drpc_queue_t queue);

void drpc_queue_close(drpc_queue_t queue);

#define DRPC_FILT_READ     0x0001
#define DRPC_FILT_WRITE    0x0010
#define DRPC_EVENT_EDGE    0x1000

int drpc_queue_add(drpc_queue_t queue, intptr_t ident,
        uint32_t flags, void* data);

#endif /* DRPC_SRC_QUEUE_H */

