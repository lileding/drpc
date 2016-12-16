#ifndef DRPC_SRC_QUEUE_H
#define DRPC_SRC_QUEUE_H

#include <sys/uio.h>

struct drpc_message;

struct drpc_queue {
    STAILQ_HEAD(, drpc_message) queue;
    struct drpc_message* current;
    struct iovec iov;
    unsigned is_body:1;
};
typedef struct drpc_queue* drpc_queue_t;

void drpc_queue_open(drpc_queue_t q);
void drpc_queue_close(drpc_queue_t q);

int drpc_queue_read(drpc_queue_t q, int fd);
int drpc_queue_write(drpc_queue_t q, int fd);

int drpc_queue_push(drpc_queue_t q, struct drpc_message* msg);
int drpc_queue_pop(drpc_queue_t q, struct drpc_message** msg);

#endif /* DRPC_SRC_QUEUE_H */

