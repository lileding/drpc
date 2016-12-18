#ifndef DRPC_SRC_SESSION_H
#define DRPC_SRC_SESSION_H

#include <stdint.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include "round.h"

struct drpc_server;

struct drpc_session {
    TAILQ_ENTRY(drpc_session) entries;
    uint64_t refcnt;
    struct drpc_server* server;
    int endpoint;
    drpc_request_t input;
    struct iovec ivec;
    pthread_mutex_t mutex;
    STAILQ_HEAD(, drpc_round) outputs;
    drpc_round_t output;
    struct iovec ovec;
    unsigned is_body:1;
};
typedef struct drpc_session* drpc_session_t;

drpc_session_t drpc_session_new(int endpoint);

void drpc_session_close(drpc_session_t sess);

void drpc_session_drop(drpc_session_t sess);

void drpc_session_process(drpc_session_t chan);

void drpc_session_send(drpc_session_t chan, drpc_round_t round);

#endif /* DRPC_SRC_SESSION_H */

