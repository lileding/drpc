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
#include "channel.h"
#include "round.h"
#include "server.h"
#include "session.h"

static void do_write(drpc_session_t sess);

drpc_session_t drpc_session_new(int endpoint, drpc_server_t server) {
    DRPC_ENSURE(server, "invalid argument");
    drpc_session_t sess = drpc_new(drpc_session);
    sess->server = server;
    sess->endpoint = endpoint;
    sess->draining = 0;
    sess->dying = 0;
    sess->input = NULL;
    sess->ivec.iov_base = NULL;
    sess->ivec.iov_len = 0;
    /* below are mutex protected */
    int err = pthread_mutex_init(&sess->mutex, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(err));
        drpc_free(sess);
        return NULL;
    }
    sess->actives = 0;
    STAILQ_INIT(&sess->pendings);
    sess->output = NULL;
    sess->ovec.iov_base = NULL;
    sess->ovec.iov_len = 0;
    sess->is_body = 0;
    return sess;
}

void drpc_session_drop(drpc_session_t sess) {
    DRPC_ENSURE(sess, "invalid argument");
    DRPC_LOG(NOTICE, "session drop [endpoint=%d]", sess->endpoint);
    pthread_mutex_destroy(&sess->mutex);
    shutdown(sess->endpoint, SHUT_RDWR);
    drpc_free(sess);
}

void drpc_session_close(drpc_session_t sess) {
    DRPC_ENSURE(sess, "invalid argument");
    DRPC_LOG(NOTICE, "session begin close [endpoint=%d]", sess->endpoint);
    pthread_mutex_lock(&sess->mutex);
    do_write(sess);
    if (sess->output == NULL && STAILQ_EMPTY(&sess->pendings)) {
        drpc_event_del(&sess->server->event, sess->endpoint);
        TAILQ_REMOVE(&sess->server->sessions, sess, entries);
        TAILQ_INSERT_TAIL(&sess->server->recycle, sess, entries);
    } else {
        sess->dying = 1; // wait for last write to recycle
    }
    pthread_mutex_unlock(&sess->mutex);
}

void drpc_session_read(drpc_session_t sess) {
    DRPC_ENSURE(sess, "invalid argument");
    if (sess->draining) {
        DRPC_LOG(WARNING, "session read but draining, exit");
        return;
    }
    int fd = sess->endpoint;
    int rv = DRPC_IO_FAIL;
    while (1) {
        if (!sess->input) {
            sess->input = drpc_new(drpc_request);
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
                sess->input->body = (char*)drpc_alloc2("reqbody", len);
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
            sess->input = NULL;
            pthread_mutex_lock(&sess->mutex);
            sess->actives++;
            pthread_mutex_unlock(&sess->mutex);
            drpc_thrpool_apply(&sess->server->pool, (drpc_task_t)round);
        }
    }
    if (sess->input) {
        if (sess->input->body) {
            drpc_free(sess->input->body);
            sess->input->body = NULL;
        }
        drpc_free(sess->input);
        sess->input = NULL;
    }
    sess->ivec.iov_base = NULL;
    sess->ivec.iov_len = 0;
    pthread_mutex_lock(&sess->mutex);
    DRPC_LOG(NOTICE, "session begin draining [endpoint=%d]", sess->endpoint);
    sess->draining = 1;
    if (sess->actives == 0) {
        drpc_channel_write(&sess->server->chan, sess);
    }
    pthread_mutex_unlock(&sess->mutex);
}

void drpc_session_write(drpc_session_t sess) {
    pthread_mutex_lock(&sess->mutex);
    do_write(sess);
    if (sess->output == NULL && STAILQ_EMPTY(&sess->pendings) && sess->dying) {
        drpc_event_del(&sess->server->event, sess->endpoint);
        TAILQ_REMOVE(&sess->server->sessions, sess, entries);
        TAILQ_INSERT_TAIL(&sess->server->recycle, sess, entries);
    }
    pthread_mutex_unlock(&sess->mutex);
}

void drpc_session_send(drpc_session_t sess, drpc_round_t round) {
    DRPC_ENSURE(sess && round, "invalid argument");
    pthread_mutex_lock(&sess->mutex);
    STAILQ_INSERT_TAIL(&sess->pendings, round, entries);
    DRPC_LOG(DEBUG, "session output enqueue [sequence=%x]",
        round->response.header.sequence);
    do_write(sess);
    sess->actives--;
    if (sess->draining && sess->actives == 0) {
        drpc_channel_write(&sess->server->chan, sess);
    }
    pthread_mutex_unlock(&sess->mutex);
}

void do_write(drpc_session_t sess) {
    int fd = sess->endpoint;
    int rv = DRPC_IO_FAIL;
    while (1) {
        if (!sess->output) {
            sess->output = STAILQ_FIRST(&sess->pendings);
            if (!sess->output) {
                DRPC_LOG(DEBUG, "session block idle [endpoint=%d]", sess->endpoint);
                break; 
            }
            STAILQ_REMOVE_HEAD(&sess->pendings, entries);
            sess->ovec.iov_base = &sess->output->response.header;
            sess->ovec.iov_len = sizeof(sess->output->response.header);
        }
        if (!sess->is_body) {
            rv = drpc_write(fd, &sess->ovec);
            if (rv == DRPC_IO_BLOCK) {
                return;
            } else if (rv == DRPC_IO_FAIL) {
                break;
            } else {
                sess->ovec.iov_base = sess->output->response.body;
                sess->ovec.iov_len = sess->output->response.header.payload;
                sess->is_body = 1;
                if (sess->output->response.header.sequence == 0) {
                    DRPC_LOG(ERROR, "session write incorrect");
                }
            }
        }
        rv = drpc_write(fd, &sess->ovec);
        if (rv == DRPC_IO_BLOCK) {
            return;
        } else if (rv == DRPC_IO_FAIL) {
            break;
        } else {
            DRPC_LOG(DEBUG, "session write message [sequence=%x]",
                sess->output->response.header.sequence);
            drpc_round_drop(sess->output);
            sess->ovec.iov_base = NULL;
            sess->ovec.iov_len = 0;
            sess->is_body = 0;
            sess->output = STAILQ_FIRST(&sess->pendings);
        }
    }
    if (sess->output) {
        drpc_round_drop(sess->output);
        sess->output = NULL;
    }
    drpc_round_t round = STAILQ_FIRST(&sess->pendings);
    drpc_round_t round2 = NULL;
    while (round) {
        round2 = STAILQ_NEXT(round, entries);
        STAILQ_REMOVE_HEAD(&sess->pendings, entries);
        drpc_round_drop(round);
        round = round2;
    }
}

