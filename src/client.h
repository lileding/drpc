#ifndef DRPC_SRC_CLIENT_H
#define DRPC_SRC_CLIENT_H

#include <pthread.h>
#include <drpc.h>
#include "queue.h"
#include "signal.h"

struct drpc_client {
    int endpoint;
    struct drpc_queue queue;
    struct drpc_signal quit;
    pthread_t thread;
};

#endif /* DRPC_SRC_CLIENT_H */

