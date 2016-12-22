#ifndef DRPC_SRC_SERVER_H
#define DRPC_SRC_SERVER_H

#include <pthread.h>
#include <sys/queue.h>
#include <drpc.h>
#include "signal.h"
#include "thrpool.h"
#include "event.h"

struct drpc_mpsc;
struct drpc_session;

struct drpc_server {
    struct drpc_task loop;
    struct {
        DRPC_EVENT_BASE(endpoint);
    } acceptor;
    drpc_select_t source;
    struct drpc_mpsc* mpsc;
    TAILQ_HEAD(, drpc_session) actives;
    TAILQ_HEAD(, drpc_session) recycle;
    struct drpc_task quit;
    struct drpc_thrpool pool;
    drpc_func stub_func;
    void* stub_arg;
};

void drpc_server_execute(drpc_server_t server, drpc_task_t task);

void drpc_server_recycle(drpc_server_t server, struct drpc_session sess);

#endif /* DRPC_SRC_SERVER_H */

