#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "io.h"
#include "mem.h"
#include "mpsc.h"
#include "session.h"
#include "server.h"

/**
 *
 * Here's the logic
 *
 * Server is made up of 2 parts: thread_pool & event
 *   - event loops according to reference count
 *     - one for accept
 *     - each one for session
 *   - thread_pool works with a thread-safe task queue
 *
**/

#define DUMP_SESSIONS(q) \
    do { \
        drpc_session_t __dump_tmp = NULL; \
        int __dump_idx = 0; \
        if (TAILQ_EMPTY(q)) { \
            DRPC_LOG(NOTICE, "DUMP session EMPTY"); \
        } else { \
            TAILQ_FOREACH(__dump_tmp, (q), entries) { \
                __dump_idx++; \
                DRPC_LOG(NOTICE, \
                    "DUMP session %03d " \
                        "[endpoint=%d] [draining=%d] [actives=%lu]", \
                    __dump_idx, \
                    __dump_tmp->endpoint, \
                    __dump_tmp->draining, \
                    __dump_tmp->actives \
                ); \
            } \
            DRPC_LOG(NOTICE, "DUMP session [pending=%d]", __dump_idx++); \
        } \
    } while (0)

static void do_event(drpc_task_t task);
static void on_accept(drpc_event_t ev, uint16_t flags);
static void on_quit(drpc_task_t task);

drpc_server_t drpc_server_new(const char* hostname, const char* servname) {
    if (!hostname || !servname) {
        DRPC_LOG(ERROR, "invalid argument");
        return NULL;
    }
    drpc_server_t serv = drpc_new(drpc_server);
    int endpoint = drpc_listen(hostname, servname, 1024);
    if (endpoint == -1) {
        drpc_free(serv);
        return NULL;
    }
    DRPC_TASK_INIT(&serv->loop, "server", do_event);
    DRPC_EVENT_INIT(&serv->acceptor,
        endpoint, on_accept, DRPC_EVENT_READ | DRPC_EVENT_EDGE);
    DRPC_TASK_INIT(&serv->quit, "server-quit", on_quit);
    TAILQ_INIT(&serv->actives);
    TAILQ_INIT(&serv->recycle);
    serv->source = drpc_select_new();
    if (!serv->source) {
        close(endpoint); 
        drpc_free(serv);
        return NULL;
    }
    if (drpc_select_add(serv->source, (drpc_event_t)&serv->acceptor) != 0) {
        DRPC_LOG(ERROR, "server add acceptor fail [ident=%d]",
            serv->acceptor.__drpc_event_ident);
        drpc_select_drop(serv->source);
        close(endpoint); 
        drpc_free(serv);
        return NULL;
    }
    serv->mpsc = drpc_mpsc_new();
    if (!serv->mpsc) {
        drpc_select_drop(serv->source);
        close(endpoint); 
        drpc_free(serv);
        return NULL;
    }
    if (drpc_select_add(serv->source, (drpc_event_t)serv->mpsc) != 0) {
        DRPC_LOG(ERROR, "server add acceptor fail [ident=%d]",
            serv->acceptor.__drpc_event_ident);
        drpc_mpsc_drop(serv->mpsc);
        drpc_select_drop(serv->source);
        close(endpoint); 
        drpc_free(serv);
    }
    if (drpc_thrpool_open(&serv->pool, 4) != 0) {
        drpc_mpsc_drop(serv->mpsc);
        drpc_select_drop(serv->source);
        close(endpoint); 
        drpc_free(serv);
        return NULL;
    }
    drpc_thrpool_apply(&serv->pool, &serv->loop);
    return serv;
}

void drpc_server_join(drpc_server_t serv) {
    DRPC_ENSURE(serv, "invalid argument");
    drpc_mpsc_send(serv->mpsc, &serv->quit);
    drpc_thrpool_join(&serv->pool);
    drpc_select_drop(serv->source);
    drpc_mpsc_drop(serv->mpsc);
    drpc_free(serv);
    DRPC_LOG(DEBUG, "server exit");
}

int drpc_server_register(drpc_server_t serv, drpc_func stub, void* arg) {
    DRPC_ENSURE(serv, "invalid argument");
    if (!stub) {
        DRPC_LOG(ERROR, "invalid argument");
        return -1;
    }
    serv->stub_func = stub;
    serv->stub_arg = arg;
    return 0;
}

void do_event(drpc_task_t task) {
    drpc_server_t serv = DRPC_VBASE(drpc_server, task, loop);
    int rv = drpc_select_wait(serv->source, NULL);
    if (rv < 0) {
        DRPC_LOG(FATAL, "server select fail, abort");
    }
    drpc_session_t sess = NULL;
    while (!TAILQ_EMPTY(&serv->recycle)) {
        sess = TAILQ_FIRST(&serv->recycle);
        TAILQ_REMOVE(&serv->recycle, sess, entries);
        drpc_session_drop(sess);
    }
    if (serv->acceptor.__drpc_event_ident == -1) {
        if (TAILQ_EMPTY(&serv->actives)) {
            drpc_thrpool_close(&serv->pool);
        } else {
            DRPC_LOG(NOTICE, "server draining");
            DUMP_SESSIONS(&serv->actives);
            drpc_thrpool_apply(&serv->pool, &serv->loop);
        }
    } else {
        drpc_thrpool_apply(&serv->pool, &serv->loop);
    }
}

void on_accept(drpc_event_t ev, uint16_t flags) {
    drpc_server_t serv = DRPC_VBASE(drpc_server, ev, acceptor);
    int sock = -1;
    while (1) {
        sock = accept(serv->acceptor.__drpc_event_ident, NULL, NULL);
        if (sock == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else if (errno == ECONNABORTED) {
                continue;
            } else {
                break;
            }
        }
        drpc_session_t sess = drpc_session_new(sock, serv);
        if (!sess) {
            shutdown(sock, SHUT_RDWR);
            continue;
        }
        TAILQ_INSERT_TAIL(&serv->actives, sess, entries);
        DRPC_LOG(DEBUG, "server accept sock=%d", sock);
    }
    DRPC_LOG(ERROR, "accept fail: %s", strerror(errno));
    shutdown(serv->acceptor.__drpc_event_ident, SHUT_RDWR);
    serv->acceptor.__drpc_event_ident = -1;
}

void on_quit(drpc_task_t task) {
    drpc_server_t serv = DRPC_VBASE(drpc_server, task, quit);
    DRPC_LOG(NOTICE, "server got quit signal");
    if (serv->acceptor.__drpc_event_ident != -1) {
        shutdown(serv->acceptor.__drpc_event_ident, SHUT_RDWR);
        serv->acceptor.__drpc_event_ident = -1;
    }
    drpc_session_t sess = NULL;
    TAILQ_FOREACH(sess, &serv->actives, entries) {
        shutdown(sess->endpoint, SHUT_RD);
        drpc_session_read(sess);
    }
}

