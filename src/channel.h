#ifndef DRPC_SRC_CHANNEL_H
#define DRPC_SRC_CHANNEL_H

#include <sys/queue.h>
#include <drpc.h>
#include "protocol.h"

struct drpc_channel;
typedef struct drpc_channel* drpc_channel_t;

struct drpc_session {
    STAILQ_ENTRY(drpc_session) entries;
    drpc_channel_t channel;
    struct drpc_message output;
    struct drpc_message input;
    struct iovec iov;
    unsigned is_body:1;
};

typedef struct drpc_session* drpc_session_t;

#define DRPC_IO_COMPLETE  0
#define DRPC_IO_BLOCK     1
#define DRPC_IO_FAIL      2

int drpc_write(int fd, struct iovec* iov);
int drpc_read(int fd, struct iovec* iov);

struct drpc_channel {
    STAILQ_ENTRY(drpc_channel) entries;
    int endpoint;
    int64_t actives;
    drpc_session_t input;
    STAILQ_HEAD(, drpc_session) sessions;
    unsigned can_write:1;
    unsigned can_read:1;
};

drpc_channel_t drpc_channel_new(int fd);

int64_t drpc_channel_process(drpc_channel_t chan, int16_t filter);

void drpc_channel_drop(drpc_channel_t chan);

void drpc_channel_shutdown_read(drpc_channel_t chan);
void drpc_channel_shutdown_write(drpc_channel_t chan);

#endif /* DRPC_SRC_CHANNEL_H */

