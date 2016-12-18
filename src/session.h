#ifndef DRPC_SRC_SESSION_H
#define DRPC_SRC_SESSION_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include "protocol.h"

struct drpc_server;

struct drpc_session {
    TAILQ_ENTRY(drpc_session) entries;
    atomic_int_least64_t refcnt;
    struct drpc_server* server;
    int endpoint;
    drpc_message_t input;
    struct iovec ivec;
    pthread_mutex_t mutex;
    STAILQ_HEAD(, drpc_message) outputs;
    drpc_message_t output;
    struct iovec ovec;
    unsigned is_body:1;
};
typedef struct drpc_session* drpc_session_t;

drpc_session_t drpc_session_new(int endpoint);

void drpc_session_process(drpc_session_t chan, int16_t events);

void drpc_session_send(drpc_session_t chan, drpc_message_t msg);

#endif /* DRPC_SRC_SESSION_H */

