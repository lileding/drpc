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
#include "session.h"
#include "channel.h"

static int drpc_channel_read(drpc_channel_t chan);
static int drpc_channel_write(drpc_channel_t chan);

drpc_channel_t drpc_channel_new(int endpoint) {
    drpc_channel_t chan = (drpc_channel_t)drpc_alloc(sizeof(*chan));
    chan->endpoint = endpoint;
    chan->input = NULL;
    chan->iov.iov_base = NULL;
    chan->iov.iov_len = 0;
    STAILQ_INIT(&chan->outputs);
    int err = pthread_mutex_init(&chan->qlock, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(err));
        drpc_channel_drop(chan);
        return NULL;
    }
    err = pthread_mutex_init(&chan->wlock, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(err));
        drpc_channel_drop(chan);
        return NULL;
    }
    chan->output = NULL;
    chan->ovec.iov_base = NULL;
    chan->ovec.iov_len = 0;
    chan->is_body = 0;
    return chan;
}

void drpc_channel_drop(drpc_channel_t chan) {
    close(chan->endpoint);
    pthread_mutex_destroy(&chan->wlock);
    pthread_mutex_destroy(&chan->qlock);
    drpc_free(chan);
}

int drpc_channel_process(drpc_channel_t chan, int16_t events) {
    DRPC_LOG(DEBUG, "channel process [endpoint=%d] [events=%u]", chan->endpoint, events);
    if (events & DRPC_EVENT_READ) {
        DRPC_LOG(DEBUG, "channel read begin [endpoint=%d]", chan->endpoint);
        int rv = drpc_channel_read(chan);
        DRPC_LOG(DEBUG, "channel read end [endpoint=%d]", chan->endpoint);
        if (rv == -1) {
            return rv;
        }
    }
    if (events & DRPC_EVENT_WRITE) {
        DRPC_LOG(DEBUG, "channel write begin [endpoint=%d]", chan->endpoint);
        pthread_mutex_lock(&chan->wlock);
        int rv = drpc_channel_write(chan);
        pthread_mutex_unlock(&chan->wlock);
        DRPC_LOG(DEBUG, "channel read end [endpoint=%d]", chan->endpoint);
        if (rv == -1) {
            return rv;
        }
    }
    return 0;
}

void drpc_channel_send(drpc_channel_t chan, drpc_message_t msg) {
    DRPC_ENSURE(chan != NULL && msg != NULL, "invalid argument");
    pthread_mutex_lock(&chan->qlock);
    STAILQ_INSERT_TAIL(&chan->outputs, msg, entries);
    DRPC_LOG(DEBUG, "channel output enqueue [sequence=%x]", msg->header.sequence);
    pthread_mutex_unlock(&chan->qlock);
    pthread_mutex_lock(&chan->wlock);
    int rv = drpc_channel_write(chan);
    pthread_mutex_unlock(&chan->wlock);
    if (rv != 0) {
        DRPC_LOG(ERROR, "channel write fail: %s", strerror(errno));
        // FIXME deal with error
    }
}

int drpc_channel_read(drpc_channel_t chan) {
    int fd = chan->endpoint;
    int rv = DRPC_IO_FAIL;
    while (1) {
        if (!chan->input) {
            chan->input = (drpc_message_t)drpc_alloc(sizeof(struct drpc_message));
            if (!chan->input) {
                DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
                return -1;
            }
            chan->input->body = NULL;
            chan->iov.iov_base = &chan->input->header;
            chan->iov.iov_len = sizeof(chan->input->header);
        }
        if (!chan->input->body) {
            rv = drpc_read(fd, &chan->iov);
            if (rv == DRPC_IO_BLOCK) {
                break;
            } else if (rv == DRPC_IO_FAIL) {
                return -1;
            } else {
                size_t len = chan->input->header.payload;
                chan->input->body = (char*)drpc_alloc(len);
                if (!chan->input->body) {
                    DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
                    return -1;
                }
                chan->iov.iov_base = chan->input->body;
                chan->iov.iov_len = len;
            }
        }
        rv = drpc_read(fd, &chan->iov);
        if (rv == DRPC_IO_BLOCK) {
            break;
        } else if (rv == DRPC_IO_FAIL) {
            return -1;
        } else {
            DRPC_LOG(DEBUG, "channel read message [sequence=%x]", chan->input->header.sequence);
            drpc_session_t sess = drpc_session_new(chan);
            if (!sess) {
                return -1;
            }
            sess->input = chan->input;
            drpc_thrpool_apply(chan->pool, (drpc_task_t)sess);
            chan->input = NULL;
        }
    }
    DRPC_LOG(DEBUG, "channel block read [endpoint=%d]", chan->endpoint);
    return 0;
}

int drpc_channel_write(drpc_channel_t chan) {
    int fd = chan->endpoint;
    int rv = DRPC_IO_FAIL;
    while (1) {
        if (chan->output == NULL) {
            pthread_mutex_lock(&chan->qlock);
            chan->output = STAILQ_FIRST(&chan->outputs);
            if (chan->output == NULL) {
                pthread_mutex_unlock(&chan->qlock);
                DRPC_LOG(DEBUG, "channel block idle [endpoint=%d]", chan->endpoint);
                return 0; 
            }
            STAILQ_REMOVE_HEAD(&chan->outputs, entries);
            pthread_mutex_unlock(&chan->qlock);
            chan->ovec.iov_base = &chan->output->header;
            chan->ovec.iov_len = sizeof(chan->output->header);
        }
        if (!chan->is_body) {
            rv = drpc_write(fd, &chan->ovec);
            if (rv == DRPC_IO_BLOCK) {
                break;
            } else if (rv == DRPC_IO_FAIL) {
                return -1;
            } else {
                chan->ovec.iov_base = chan->output->body;
                chan->ovec.iov_len = chan->output->header.payload;
                chan->is_body = 1;
            }
        }
        rv = drpc_write(fd, &chan->ovec);
        if (rv == DRPC_IO_BLOCK) {
            break;
        } else if (rv == DRPC_IO_FAIL) {
            return -1;
        } else {
            DRPC_LOG(DEBUG, "channel write message [sequence=%x]", chan->output->header.sequence);
            chan->output = NULL;
            chan->ovec.iov_base = NULL;
            chan->ovec.iov_len = 0;
            chan->is_body = 0;
        }
    }
    DRPC_LOG(DEBUG, "channel block write [endpoint=%d]", chan->endpoint);
    return 0;
}

#if 0
static void on_write(drpc_channel_t chan);
static int drpc_channel_read(drpc_channel_t chan, drpc_session_t sess);
static int drpc_channel_write(drpc_channel_t chan, drpc_session_t sess);

drpc_channel_t drpc_channel_new(int fd) {
    DRPC_ENSURE_OR(fd != -1, NULL, "invalid argument");
    if (drpc_set_nonblock(fd) == -1) {
        DRPC_LOG(ERROR, "set nonblock fail: %s", strerror(errno));
        return NULL;
    }
    drpc_channel_t chan = (drpc_channel_t)drpc_alloc(sizeof(*chan));
    if (!chan) {
        DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
        return NULL;
    }
    chan->endpoint = fd;
    chan->input = drpc_session_new(chan);
    if (!chan->input) {
        DRPC_LOG(ERROR, "create session fail: %s", strerror(errno));
        drpc_channel_drop(chan);
        return NULL;
    }
    STAILQ_INIT(&chan->sessions);
    chan->client = NULL;
    chan->actives = 0;
    chan->can_read = 1;
    chan->can_write = 1;
    return chan;
}

void drpc_channel_drop(drpc_channel_t chan) {
    if (!chan) {
        return;
    }
    if (chan->endpoint != -1) {
        close(chan->endpoint);
    }
    drpc_session_drop(chan->input);
    drpc_session_t sess = NULL;
    STAILQ_FOREACH(sess, &chan->sessions, entries) {
        drpc_session_drop(sess);
    }
    drpc_free(chan);
}

int64_t drpc_channel_process(drpc_channel_t chan, int16_t filter) {
    DRPC_ENSURE_OR(chan, -1, "invalid argument");
    if (filter & EVFILT_READ) {
        DRPC_LOG(DEBUG, "read event begin [client=%p]", chan->client);
        while (chan->can_read) {
            int rv = drpc_channel_read(chan, chan->input);
            if (rv == DRPC_IO_BLOCK) {
                //DRPC_LOG(DEBUG, "read block [actives=%zu]", chan->input->iov.iov_len);
                break;
            } else if (rv == DRPC_IO_FAIL) {
                DRPC_LOG(ERROR, "channel input fail: %s", strerror(errno));
                drpc_channel_shutdown_read(chan);
                break;
            }
            chan->actives++;
            drpc_session_process(chan->input);
            chan->input = drpc_session_new(chan);
            if (!chan->input) {
                DRPC_LOG(ERROR, "create session fail: %s", strerror(errno));
                drpc_channel_shutdown_read(chan);
                break;
            }
        }
        //DRPC_LOG(DEBUG, "read event end");
    }
    if (filter & EVFILT_WRITE) {
        on_write(chan);
    }
    if (!chan->can_read) {
        if (!chan->can_write) {
            DRPC_LOG(DEBUG, "channel bad [actives=%ld]", chan->actives);
            return -1;
        } else {
            DRPC_LOG(DEBUG, "channel wait done [actives=%ld]", chan->actives);
        }
    }
    return chan->actives;
}

void on_write(drpc_channel_t chan) {
    // FIXME NOT thread safe!!!
    DRPC_LOG(DEBUG, "write event begin [actives=%lu] [client=%p]", chan->actives, chan->client);
    while (chan->can_write && !STAILQ_EMPTY(&chan->sessions)) {
        drpc_session_t sess = STAILQ_FIRST(&chan->sessions);
        int rv = drpc_channel_write(chan, sess);
        if (rv == DRPC_IO_BLOCK) {
            DRPC_LOG(DEBUG, "write block [actives=%zu]", sess->iov.iov_len);
            break;
        } else if (rv == DRPC_IO_FAIL) {
            DRPC_LOG(ERROR, "channel output fail: %s", strerror(errno));
            drpc_channel_shutdown_write(chan);
            break;
        }
        STAILQ_REMOVE_HEAD(&chan->sessions, entries);
        drpc_session_drop(sess);
        chan->actives--;
    }
    DRPC_LOG(DEBUG, "write event end");
}

void drpc_channel_call(drpc_channel_t chan, const char* request, size_t len,
        void (*callback)(const char*, size_t)) {
    drpc_session_t sess = drpc_session_new(chan);
    // FIXME
    if (!sess) {
        DRPC_LOG(ERROR, "create session fail");
        return;
    }
    drpc_message_t input = &sess->input;
    input->header.version = 0;
    input->header.compress = 0;
    input->header.sequence = 123;
    input->header.payload = len;
    input->body = (char*)request;
    drpc_channel_send(chan, sess);
}

void drpc_channel_send(drpc_channel_t chan, drpc_session_t sess) {
    sess->iov.iov_base = &sess->output.header;
    sess->iov.iov_len = sizeof(sess->output.header);
    sess->is_body = 0;
    STAILQ_INSERT_TAIL(&chan->sessions, sess, entries);
    DRPC_LOG(DEBUG, "channel add session");
    on_write(chan);
}

int drpc_channel_read(drpc_channel_t chan, drpc_session_t sess) {
    int fd = chan->endpoint;
    drpc_message_t msg = &sess->input;
    if (sess->is_body == 0) {
        int rv = drpc_read(fd, &sess->iov);
        if (rv != DRPC_IO_COMPLETE) {
            return rv;
        }
        msg->body = (char*)drpc_alloc(msg->header.payload);
        if (!msg->body) {
            DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
            return DRPC_IO_FAIL;
        }
        sess->iov.iov_base = msg->body;
        sess->iov.iov_len = msg->header.payload;
        sess->is_body = 1;
        //DRPC_LOG(DEBUG, "input header ok");
    }
    return drpc_read(fd, &sess->iov);
}

int drpc_channel_write(drpc_channel_t chan, drpc_session_t sess) {
    int fd = chan->endpoint;
    drpc_message_t msg = &sess->output;
    //DRPC_LOG(DEBUG, "sent [sock=%d] [seq=%u]", fd, msg->header.sequence);
    if (sess->is_body == 0) {
        int rv = drpc_write(fd, &sess->iov);
        if (rv != DRPC_IO_COMPLETE) {
            return rv;
        }
        sess->iov.iov_base = msg->body;
        sess->iov.iov_len = msg->header.payload;
        sess->is_body = 1;
    }
    //DRPC_LOG(DEBUG, "sent [sock=%d] [seq=%u] [payload=%u]", fd, msg->header.sequence, msg->header.payload);
    return drpc_write(fd, &sess->iov);
}

void drpc_channel_shutdown_read(drpc_channel_t chan) {
    if (!chan) {
        return;
    }
    if (chan->can_read) {
        chan->can_read = 0;
        shutdown(chan->endpoint, SHUT_RD);
    }
}

void drpc_channel_shutdown_write(drpc_channel_t chan) {
    if (!chan) {
        return;
    }
    if (chan->can_write) {
        chan->can_write = 0;
        shutdown(chan->endpoint, SHUT_WR);
    }
}
#endif

