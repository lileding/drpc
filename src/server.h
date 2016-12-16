#ifndef DRPC_SRC_SERVER_H
#define DRPC_SRC_SERVER_H

#include <stdint.h>
#include <pthread.h>
#include <sys/queue.h>
#include <drpc.h>
#include "event.h"
#include "signal.h"
#include "thrpool.h"

struct drpc_server {
    int endpoint;
    struct drpc_event event;
    drpc_signal_t quit;
    struct drpc_thrpool pool;
    struct drpc_task iotask;
    STAILQ_HEAD(, drpc_channel) channels;
    volatile uint64_t actives;
};

#endif /* DRPC_SRC_SERVER_H */

