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
#include "session.h"
#include "channel.h"

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

