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
    struct drpc_server* server;
    int endpoint;
    drpc_request_t input;
    struct iovec ivec;
    /* BEGIN mutex protected */
    pthread_mutex_t mutex;
    uint64_t actives;
    STAILQ_HEAD(, drpc_round) pendings;
    drpc_round_t output;
    struct iovec ovec;
    unsigned is_body:1;
    /* END mutex protected */
    unsigned draining:1;
    unsigned dying:1;
};
typedef struct drpc_session* drpc_session_t;

drpc_session_t drpc_session_new(int endpoint, struct drpc_server* server);

/* thread safe with server, called by server */
void drpc_session_close(drpc_session_t sess);

/* thread safe with server, called by server */
void drpc_session_drop(drpc_session_t sess);

/* thread safe with server, called by server */
void drpc_session_read(drpc_session_t chan);

/* should be protected by mutex, called by server */
void drpc_session_write(drpc_session_t chan);

/* should be protected by mutex, callback by round */
void drpc_session_send(drpc_session_t chan, drpc_round_t round);

#endif /* DRPC_SRC_SESSION_H */

