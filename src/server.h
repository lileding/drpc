#ifndef DRPC_SRC_SERVER_H
#define DRPC_SRC_SERVER_H

#include <pthread.h>
#include <sys/queue.h>
#include <drpc.h>
#include "event.h"
#include "signal.h"
#include "thrpool.h"
#include "channel.h"

struct drpc_session;

struct drpc_server {
    DRPC_TASK_BASE;
    drpc_signal_t quit;
    struct drpc_channel chan;
    struct drpc_event event;
    int endpoint;
    TAILQ_HEAD(, drpc_session) sessions;
    TAILQ_HEAD(, drpc_session) recycle;
    struct drpc_thrpool pool;
    drpc_func stub_func;
    void* stub_arg;
};

#endif /* DRPC_SRC_SERVER_H */

