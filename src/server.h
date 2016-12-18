#ifndef DRPC_SRC_SERVER_H
#define DRPC_SRC_SERVER_H

#include <stdatomic.h>
#include <pthread.h>
#include <sys/queue.h>
#include <drpc.h>
#include "event.h"
#include "signal.h"
#include "thrpool.h"

struct drpc_session;

struct drpc_server {
    DRPC_TASK_BASE;
    atomic_uint_least64_t refcnt;
    drpc_signal_t quit;
    struct drpc_event event;
    int endpoint;
    pthread_mutex_t mutex;
    TAILQ_HEAD(, drpc_session) sessions;
    struct drpc_thrpool pool;
};

void drpc_server_remove(drpc_server_t server, struct drpc_session* sess);

#endif /* DRPC_SRC_SERVER_H */

