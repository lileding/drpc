#ifndef DRPC_SRC_CHANNEL_H
#define DRPC_SRC_CHANNEL_H

#include <stdint.h>
#include <sys/queue.h>

struct drpc_client;
struct drpc_session;

struct drpc_channel;
typedef struct drpc_channel* drpc_channel_t;

struct drpc_message;
struct drpc_queue {
    STAILQ_ENTRY(drpc_message) queue;
    struct iovec iov;
    unsigned is_body:0;
};
typedef struct drpc_queue* drpc_queue_t;
void drpc_queue_open(drpc_queue_t w);
void drpc_queue_close(drpc_queue_t w);
int drpc_queue_write(drpc_queue_t w, int fd);
void drpc_queue_push(drpc_queue_t w, struct drpc_message* msg);

struct drpc_channel {
    int endpoint;
    struct drpc_client* client;
    STAILQ_ENTRY(drpc_channel) entries;
    int64_t actives;
    struct drpc_session* input;
    STAILQ_HEAD(, drpc_session) pendings;
    struct drpc_queue writer;
    unsigned can_write:1;
    unsigned can_read:1;
};

drpc_channel_t drpc_channel_new(int fd);
void drpc_channel_drop(drpc_channel_t chan);

int64_t drpc_channel_process(drpc_channel_t chan, int16_t filter);
void drpc_channel_send(drpc_channel_t chan, struct drpc_session* sess);

void drpc_channel_shutdown_read(drpc_channel_t chan);
void drpc_channel_shutdown_write(drpc_channel_t chan);

#endif /* DRPC_SRC_CHANNEL_H */

