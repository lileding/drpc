#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include "io.h"
#include "mem.h"
#include "server.h"
#include "session.h"

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

static void do_event(void* arg);
static void on_accept(drpc_server_t server);
static void on_quit(drpc_server_t server);
static void on_remove(drpc_server_t server);

drpc_server_t drpc_server_new(const char* hostname, const char* servname) {
    if (!hostname || !servname) {
        DRPC_LOG(ERROR, "invalid argument");
        return NULL;
    }
    drpc_server_t server = drpc_new(drpc_server);
    if (!server) {
        return NULL;
    }
    DRPC_TASK_INIT(server, "server", do_event);
    TAILQ_INIT(&server->sessions);
    TAILQ_INIT(&server->recycle);
    server->quit = drpc_signal_new();
    if (!server->quit) {
        drpc_free(server);
        return NULL;
    }
    if (drpc_channel_open(&server->chan) != 0) {
        drpc_signal_drop(server->quit);
        drpc_free(server);
        return NULL;
    }
    server->endpoint = drpc_listen(hostname, servname, 1024);
    if (server->endpoint == -1) {
        drpc_channel_close(&server->chan);
        drpc_signal_drop(server->quit);
        drpc_free(server);
        return NULL;
    }
    if (drpc_event_open(&server->event) != 0) {
        drpc_channel_close(&server->chan);
        shutdown(server->endpoint, SHUT_RDWR);
        drpc_signal_drop(server->quit);
        drpc_free(server);
        return NULL;
    }
    if (drpc_event_add(&server->event, drpc_signal_yield(server->quit),
                DRPC_EVENT_READ | DRPC_EVENT_ONESHOT, NULL) != 0) {
        drpc_channel_close(&server->chan);
        drpc_event_close(&server->event);
        shutdown(server->endpoint, SHUT_RDWR);
        drpc_signal_drop(server->quit);
        drpc_free(server);
        return NULL;
    }
    if (drpc_event_add(&server->event, server->chan.receive,
                DRPC_EVENT_READ, NULL) != 0) {
        drpc_channel_close(&server->chan);
        drpc_event_close(&server->event);
        shutdown(server->endpoint, SHUT_RDWR);
        drpc_signal_drop(server->quit);
        drpc_free(server);
        return NULL;
    }
    if (drpc_event_add(&server->event, server->endpoint,
                DRPC_EVENT_READ, NULL) != 0) {
        drpc_channel_close(&server->chan);
        drpc_event_close(&server->event);
        shutdown(server->endpoint, SHUT_RDWR);
        drpc_signal_drop(server->quit);
        drpc_free(server);
        return NULL;
    }
    if (drpc_thrpool_open(&server->pool, 4) != 0) {
        drpc_channel_close(&server->chan);
        drpc_event_close(&server->event);
        shutdown(server->endpoint, SHUT_RDWR);
        drpc_signal_drop(server->quit);
        drpc_free(server);
        return NULL;
    }
    drpc_thrpool_apply(&server->pool, (drpc_task_t)server);
    return server;
}

void drpc_server_join(drpc_server_t server) {
    DRPC_ENSURE(server, "invalid argument");
    drpc_signal_notify(server->quit);
    drpc_thrpool_join(&server->pool);
    drpc_event_close(&server->event);
    drpc_channel_close(&server->chan);
    drpc_signal_drop(server->quit);
    drpc_free(server);
    DRPC_LOG(DEBUG, "server exit");
}

int drpc_server_register(drpc_server_t server, drpc_func stub, void* arg) {
    DRPC_ENSURE(server, "invalid argument");
    if (!stub) {
        DRPC_LOG(ERROR, "invalid argument");
        return -1;
    }
    server->stub_func = stub;
    server->stub_arg = arg;
    return 0;
}

void do_event(void* arg) {
    drpc_server_t server = (drpc_server_t)arg;
    DRPC_LOG(DEBUG, "server wait [endpoint=%d] [event=%d]",
        server->endpoint, server->event.kq
    );
    int rv = kevent(
        server->event.kq,
        NULL, 0,
        server->event.evs, DRPC_EVENT_LIMIT,
        NULL
    );
    if (rv < 0) {
        DRPC_LOG(ERROR, "event fail: %s", strerror(errno));
        on_quit(server);
    }
    for (int i = 0; i < rv; i++) {
        const struct kevent *ev = &server->event.evs[i];
        if (ev->ident == drpc_signal_yield(server->quit)) {
            on_quit(server);
        } else if (ev->ident == server->chan.receive) {
            on_remove(server);
        } else if (ev->ident == server->endpoint) {
            on_accept(server);
        } else {
            drpc_session_t sess = (drpc_session_t)ev->udata;
            if (ev->filter & EVFILT_READ) {
                drpc_session_read(sess);
            }
            if (ev->filter & EVFILT_WRITE) {
                drpc_session_write(sess);
            }
        }
    }
    drpc_session_t sess = NULL;
    while (!TAILQ_EMPTY(&server->recycle)) {
        sess = TAILQ_FIRST(&server->recycle);
        TAILQ_REMOVE(&server->recycle, sess, entries);
        drpc_session_drop(sess);
    }
    if (server->endpoint == -1) {
        if (TAILQ_EMPTY(&server->sessions)) {
            drpc_thrpool_close(&server->pool);
        } else {
            DRPC_LOG(NOTICE, "server draining");
            DUMP_SESSIONS(&server->sessions);
            drpc_thrpool_apply(&server->pool, (drpc_task_t)server);
        }
    } else {
        drpc_thrpool_apply(&server->pool, (drpc_task_t)server);
    }
}

void on_accept(drpc_server_t server) {
    int sock = -1;
    while (1) {
        sock = accept(server->endpoint, NULL, NULL);
        if (sock == -1) {
            if (errno == EAGAIN) {
                return;
            } else if (errno == ECONNABORTED) {
                continue;
            } else {
                break;
            }
        }
        drpc_session_t sess = drpc_session_new(sock, server);
        if (!sess) {
            shutdown(sock, SHUT_RDWR);
            continue;
        }
        sess->server = server;
        if (drpc_event_add(&server->event, sock,
                DRPC_EVENT_READ | DRPC_EVENT_WRITE | DRPC_EVENT_EDGE,
                sess) != 0) {
            drpc_session_drop(sess);
            continue;
        }
        TAILQ_INSERT_TAIL(&server->sessions, sess, entries);
        DRPC_LOG(DEBUG, "server accept sock=%d", sock);
    }
    DRPC_LOG(ERROR, "accept fail: %s", strerror(errno));
    shutdown(server->endpoint, SHUT_RDWR);
    server->endpoint = -1;
}

void on_remove(drpc_server_t server) {
    drpc_channel_wait(&server->chan);
    while (1) {
        drpc_session_t sess = drpc_channel_read(&server->chan);
        if (!sess) {
            break;
        }
        drpc_session_close(sess);
    }
}

void on_quit(drpc_server_t server) {
    DRPC_LOG(NOTICE, "server got quit signal");
    if (server->endpoint != -1) {
        shutdown(server->endpoint, SHUT_RDWR);
        server->endpoint = -1;
    }
    drpc_session_t sess = NULL;
    TAILQ_FOREACH(sess, &server->sessions, entries) {
        shutdown(sess->endpoint, SHUT_RD);
        drpc_session_read(sess);
    }
}

