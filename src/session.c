#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <drpc.h>
#include "io.h"
#include "mem.h"
#include "event.h"
#include "round.h"
#include "server.h"
#include "session.h"

static void do_addref(drpc_session_t sess);
static void do_release(drpc_session_t sess);
static void do_read(drpc_session_t sess);
static void do_write(drpc_session_t sess);

drpc_session_t drpc_session_new(int endpoint) {
    drpc_session_t sess = (drpc_session_t)drpc_alloc(sizeof(*sess));
    atomic_init(&sess->refcnt, 1);
    sess->endpoint = endpoint;
    sess->input = NULL;
    sess->ivec.iov_base = NULL;
    sess->ivec.iov_len = 0;
    STAILQ_INIT(&sess->outputs);
    int err = pthread_mutex_init(&sess->mutex, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(err));
        // FIXME
        return NULL;
    }
    sess->output = NULL;
    sess->ovec.iov_base = NULL;
    sess->ovec.iov_len = 0;
    sess->is_body = 0;
    return sess;
}

void drpc_session_process(drpc_session_t sess, int16_t events) {
    DRPC_LOG(DEBUG, "session process [endpoint=%d] [events=%u]", sess->endpoint, events);
    if (events & DRPC_EVENT_READ) {
        do_read(sess);
    }
    if (events & DRPC_EVENT_WRITE) {
        pthread_mutex_lock(&sess->mutex);
        do_write(sess);
        pthread_mutex_unlock(&sess->mutex);
    }
}

void drpc_session_send(drpc_session_t sess, drpc_message_t msg) {
    DRPC_ENSURE(sess != NULL && msg != NULL, "invalid argument");
    pthread_mutex_lock(&sess->mutex);
    STAILQ_INSERT_TAIL(&sess->outputs, msg, entries);
    DRPC_LOG(DEBUG, "session output enqueue [sequence=%x]", msg->header.sequence);
    do_write(sess);
    pthread_mutex_unlock(&sess->mutex);
}

void do_read(drpc_session_t sess) {
    int fd = sess->endpoint;
    int rv = DRPC_IO_FAIL;
    while (1) {
        if (!sess->input) {
            sess->input = (drpc_message_t)drpc_alloc(sizeof(struct drpc_message));
            if (!sess->input) {
                DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
                break;
            }
            sess->input->body = NULL;
            sess->ivec.iov_base = &sess->input->header;
            sess->ivec.iov_len = sizeof(sess->input->header);
        }
        if (!sess->input->body) {
            rv = drpc_read(fd, &sess->ivec);
            if (rv == DRPC_IO_BLOCK) {
                return;
            } else if (rv == DRPC_IO_FAIL) {
                break;
            } else {
                size_t len = sess->input->header.payload;
                sess->input->body = (char*)drpc_alloc(len);
                if (!sess->input->body) {
                    DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
                    break;
                }
                sess->ivec.iov_base = sess->input->body;
                sess->ivec.iov_len = len;
            }
        }
        rv = drpc_read(fd, &sess->ivec);
        if (rv == DRPC_IO_BLOCK) {
            return;
        } else if (rv == DRPC_IO_FAIL) {
            break;
        } else {
            DRPC_LOG(DEBUG, "session read message [sequence=%x]",
                sess->input->header.sequence);
            drpc_round_t round = drpc_round_new(sess);
            if (!round) {
                break;
            }
            round->input = sess->input;
            sess->input = NULL;
            drpc_thrpool_apply(&sess->server->pool, (drpc_task_t)round);
        }
    }
    do_release(sess);
}

void do_write(drpc_session_t sess) {
    int fd = sess->endpoint;
    int rv = DRPC_IO_FAIL;
    while (1) {
        if (sess->output == NULL) {
            sess->output = STAILQ_FIRST(&sess->outputs);
            if (sess->output == NULL) {
                DRPC_LOG(DEBUG, "session block idle [endpoint=%d]", sess->endpoint);
                return; 
            }
            STAILQ_REMOVE_HEAD(&sess->outputs, entries);
            sess->ovec.iov_base = &sess->output->header;
            sess->ovec.iov_len = sizeof(sess->output->header);
        }
        if (!sess->is_body) {
            rv = drpc_write(fd, &sess->ovec);
            if (rv == DRPC_IO_BLOCK) {
                return;
            } else if (rv == DRPC_IO_FAIL) {
                break;
            } else {
                sess->ovec.iov_base = sess->output->body;
                sess->ovec.iov_len = sess->output->header.payload;
                sess->is_body = 1;
            }
        }
        rv = drpc_write(fd, &sess->ovec);
        if (rv == DRPC_IO_BLOCK) {
            return;
        } else if (rv == DRPC_IO_FAIL) {
            break;
        } else {
            DRPC_LOG(DEBUG, "session write message [sequence=%x]",
                sess->output->header.sequence);
            sess->output = NULL;
            sess->ovec.iov_base = NULL;
            sess->ovec.iov_len = 0;
            sess->is_body = 0;
        }
    }
    do_release(sess);
}

void do_addref(drpc_session_t sess) {
    atomic_fetch_add(&sess->refcnt, 1);
}

void do_release(drpc_session_t sess) {
    if (atomic_fetch_sub(&sess->refcnt, 1) > 1) {
        return;
    }
    drpc_server_remove(sess->server, sess);
    // FIXME release resource
}

