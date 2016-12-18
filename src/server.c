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

static void do_addref(drpc_server_t server);
static int do_release(drpc_server_t server);
static void do_event(void* arg);
static void on_accept(drpc_server_t server);
static void on_quit(drpc_server_t server);

drpc_server_t drpc_server_new(const char* hostname, const char* servname) {
    DRPC_ENSURE_OR(hostname && servname, NULL, "invalid argument");
    drpc_server_t server = (drpc_server_t)drpc_alloc(sizeof(*server));
    if (!server) {
        return NULL;
    }
    DRPC_TASK_INIT(server, "server", do_event);
    atomic_init(&server->refcnt, 1);
    TAILQ_INIT(&server->sessions);
    int err = pthread_mutex_init(&server->mutex, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(err));
        drpc_free(server);
        return NULL;
    }
    server->quit = drpc_signal_new();
    if (!server->quit) {
        pthread_mutex_destroy(&server->mutex);
        drpc_free(server);
        return NULL;
    }
    server->endpoint = drpc_listen(hostname, servname, 1024);
    if (server->endpoint == -1) {
        drpc_signal_drop(server->quit);
        pthread_mutex_destroy(&server->mutex);
        drpc_free(server);
        return NULL;
    }
    if (drpc_event_open(&server->event) != 0) {
        close(server->endpoint);
        drpc_signal_drop(server->quit);
        pthread_mutex_destroy(&server->mutex);
        drpc_free(server);
        return NULL;
    }
    if (drpc_event_add(&server->event, drpc_signal_yield(server->quit),
                DRPC_EVENT_READ | DRPC_EVENT_ONESHOT, NULL) != 0) {
        drpc_event_close(&server->event);
        close(server->endpoint);
        drpc_signal_drop(server->quit);
        pthread_mutex_destroy(&server->mutex);
        drpc_free(server);
        return NULL;
    }
    if (drpc_event_add(&server->event, server->endpoint,
                DRPC_EVENT_READ, NULL) != 0) {
        drpc_event_close(&server->event);
        close(server->endpoint);
        drpc_signal_drop(server->quit);
        pthread_mutex_destroy(&server->mutex);
        drpc_free(server);
        return NULL;
    }
    if (drpc_thrpool_open(&server->pool, 4) != 0) {
        drpc_event_close(&server->event);
        close(server->endpoint);
        drpc_signal_drop(server->quit);
        pthread_mutex_destroy(&server->mutex);
        drpc_free(server);
        return NULL;
    }
    drpc_thrpool_apply(&server->pool, (drpc_task_t)server);
    return server;
}

void drpc_server_join(drpc_server_t server) {
    DRPC_ENSURE(server != NULL, "invalid argument");
    drpc_signal_notify(server->quit);
    drpc_thrpool_join(&server->pool);
    drpc_free(server);
    DRPC_LOG(DEBUG, "server exit");
}

void drpc_server_remove(drpc_server_t server, drpc_session_t sess) {
    DRPC_ENSURE(server != NULL && sess != NULL, "invalid argument");
    DRPC_LOG(NOTICE, "server remove session");
    pthread_mutex_lock(&server->mutex);
    TAILQ_REMOVE(&server->sessions, sess, entries);
    if (TAILQ_EMPTY(&server->sessions)) {
        // FIXME do_release(server);
    }
    pthread_mutex_unlock(&server->mutex);
    DRPC_LOG(NOTICE, "server remove session done");
}

void do_addref(drpc_server_t server) {
    atomic_fetch_add_explicit(&server->refcnt, 1, memory_order_seq_cst);
}

int do_release(drpc_server_t server) {
    if (atomic_fetch_sub_explicit(&server->refcnt, 1, memory_order_seq_cst) > 1) {
        return 0;
    }
    DRPC_LOG(NOTICE, "server event close");
    drpc_event_close(&server->event);
    if (server->endpoint != -1) {
        close(server->endpoint);
    }
    pthread_mutex_destroy(&server->mutex);
    drpc_signal_drop(server->quit);
    drpc_thrpool_close(&server->pool);
    return 1;
}

void do_event(void* arg) {
    drpc_server_t server = (drpc_server_t)arg;
    pthread_mutex_lock(&server->mutex);
    //do_addref(server);
    DRPC_LOG(DEBUG, "server wait [endpoint=%d] [event=%d] [refcnt=%zu]",
        server->endpoint, server->event.kq, atomic_load(&server->refcnt)
    );
    struct timespec timeout = { 1, 0 };
    int rv = kevent(server->event.kq, NULL, 0, server->event.evs, DRPC_EVENT_LIMIT, &timeout);
    DRPC_LOG(NOTICE, "kevent return %d", rv);
    if (rv < 0) {
        DRPC_LOG(ERROR, "event fail: %s", strerror(errno));
        on_quit(server);
    }
    for (int i = 0; i < rv; i++) {
        const struct kevent *ev = &server->event.evs[i];
        if (ev->ident == drpc_signal_yield(server->quit)) {
            on_quit(server);
        } else if (ev->ident == server->endpoint) {
            on_accept(server);
        } else {
            drpc_session_t sess = (drpc_session_t)ev->udata;
            drpc_session_process(sess, ev->filter);
        }
    }
    //do_release(server);
    /* next task would be omitted if event was released */
    if (server->endpoint != -1 || !TAILQ_EMPTY(&server->sessions)) {
        DRPC_LOG(NOTICE, "server continue [endpoint=%d] [1st.sess.fd=%d]", server->endpoint, TAILQ_EMPTY(&server->sessions) ? -1 : TAILQ_FIRST(&server->sessions)->endpoint);
        drpc_thrpool_apply(&server->pool, (drpc_task_t)server);
    } else {
        drpc_thrpool_close(&server->pool);
    }
    pthread_mutex_unlock(&server->mutex);
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
        drpc_session_t sess = drpc_session_new(sock);
        if (!sess) {
            close(sock);
            continue;
        }
        sess->server = server;
        if (drpc_event_add(&server->event, sock,
                    DRPC_EVENT_READ | DRPC_EVENT_WRITE | DRPC_EVENT_EDGE,
                    sess) != 0) {
            // FIXME drpc_session_drop(sess);
            continue;
        }
        //pthread_mutex_lock(&server->mutex);
        if (TAILQ_EMPTY(&server->sessions)) {
            do_addref(server);
        }
        TAILQ_INSERT_TAIL(&server->sessions, sess, entries);
        //pthread_mutex_unlock(&server->mutex);
        DRPC_LOG(DEBUG, "server accept sock=%d", sock);
    }
    DRPC_LOG(ERROR, "accept fail: %s", strerror(errno));
    close(server->endpoint);
    server->endpoint = -1;
    //do_release(server);
}

void on_quit(drpc_server_t server) {
    DRPC_LOG(NOTICE, "server got quit signal");
    if (server->endpoint != -1) {
        close(server->endpoint);
        server->endpoint = -1;
        //do_release(server);
    }
    drpc_session_t sess = NULL;
    //pthread_mutex_lock(&server->mutex);
    TAILQ_FOREACH(sess, &server->sessions, entries) {
        shutdown(sess->endpoint, SHUT_RD);
    }
    DRPC_LOG(NOTICE, "server begin draining");
    //pthread_mutex_unlock(&server->mutex);
    /* graceful exit, response all received requests */
}

