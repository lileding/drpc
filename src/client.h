#ifndef DRPC_SRC_CLIENT_H
#define DRPC_SRC_CLIENT_H

#include <stddef.h>
#include <pthread.h>
#include <sys/queue.h>
#include <drpc.h>
#include "event.h"
#include "signal.h"

struct drpc_client {
    struct drpc_event event;
    drpc_signal_t quit;
    pthread_t* threads;
    size_t thread_num;
    STAILQ_HEAD(, drpc_channel) channels;
    uint64_t actives;
};

#endif /* DRPC_SRC_CLIENT_H */

