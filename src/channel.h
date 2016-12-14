#ifndef DRPC_SRC_CHANNEL_H
#define DRPC_SRC_CHANNEL_H

#include <drpc.h>
#include "protocol.h"

struct drpc_channel;
typedef struct drpc_channel* drpc_channel_t;

struct drpc_session {
    drpc_channel_t channel;
    STAILQ_ENTRY(drpc_session) entries;
    struct drpc_message send;
    struct drpc_message receive;
    struct iovec iov;
    unsigned is_body:1;
};

typedef struct drpc_session* drpc_session_t;

#define DRPC_IO_COMPLETE  0
#define DRPC_IO_BLOCK     1
#define DRPC_IO_FAIL      2

int drpc_send(int fd, struct iovec* iov);
int drpc_recv(int fd, struct iovec* iov);

struct drpc_channel {
    int endpoint;
    drpc_session_t receive;
    STAILQ_HEAD(, drpc_session) send_queue;
    unsigned can_send:1;
    unsigned can_receive:1;
};

drpc_channel_t drpc_channel_new(int fd);

int drpc_channel_process(drpc_channel_t chan, int16_t filter);

void drpc_channel_drop(drpc_channel_t chan);


#endif /* DRPC_SRC_CHANNEL_H */

