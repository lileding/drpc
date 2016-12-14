#ifndef DRPC_SRC_SERVER_H
#define DRPC_SRC_SERVER_H

#include <stdint.h>
#include <pthread.h>
#include <sys/queue.h>
#include <drpc.h>
#include "queue.h"
#include "signal.h"

struct drpc_server {
    int endpoint;
    struct drpc_queue queue;
    drpc_signal_t quit;
    pthread_t thread;
    STAILQ_HEAD(, drpc_channel) channels;
    uint64_t actives;
};

#endif /* DRPC_SRC_SERVER_H */

