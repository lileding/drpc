#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/event.h>
#include <sys/socket.h>
#include "mem.h"
#include "endpoint.h"
#include "channel.h"

static drpc_session_t drpc_session_new(drpc_channel_t chan);
static int drpc_session_write(drpc_session_t sess);
static int drpc_session_read(drpc_session_t sess);
static void drpc_session_process(drpc_session_t sess);
static void drpc_session_drop(drpc_session_t sess);

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
        //DRPC_LOG(DEBUG, "read event begin");
        while (chan->can_read) {
            int rv = drpc_session_read(chan->input);
            if (rv == DRPC_IO_BLOCK) {
                //DRPC_LOG(DEBUG, "read block [actives=%zu]", chan->input->iov.iov_len);
                break;
            } else if (rv == DRPC_IO_FAIL) {
                DRPC_LOG(ERROR, "channel input fail: %s", strerror(errno));
                drpc_channel_shutdown_read(chan);
                if (!chan->can_write) {
                    return -1;
                }
                break;
            }
            chan->actives++;
            drpc_session_process(chan->input);
            chan->input = drpc_session_new(chan);
            if (!chan->input) {
                DRPC_LOG(ERROR, "create session fail: %s", strerror(errno));
                drpc_channel_shutdown_read(chan);
                if (!chan->can_write) {
                    return -1;
                }
                break;
            }
        }
        //DRPC_LOG(DEBUG, "read event end");
    }
    if (filter & EVFILT_WRITE) {
        //DRPC_LOG(DEBUG, "write event begin [actives=%lu]", chan->actives);
        while (chan->can_write && !STAILQ_EMPTY(&chan->sessions)) {
            drpc_session_t sess = STAILQ_FIRST(&chan->sessions);
            int rv = drpc_session_write(sess);
            if (rv == DRPC_IO_BLOCK) {
                //DRPC_LOG(DEBUG, "write block [actives=%zu]", sess->iov.iov_len);
                break;
            } else if (rv == DRPC_IO_FAIL) {
                DRPC_LOG(ERROR, "channel output fail: %s", strerror(errno));
                drpc_channel_shutdown_write(chan);
                if (!chan->can_read) {
                    return -1;
                }
                break;
            }
            STAILQ_REMOVE_HEAD(&chan->sessions, entries);
            drpc_session_drop(sess);
            chan->actives--;
        }
        //DRPC_LOG(DEBUG, "write event end");
    }
    if (!chan->can_read) {
        DRPC_LOG(DEBUG, "channel wait done [actives=%ld]", chan->actives);
    }
    return chan->actives;
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

drpc_session_t drpc_session_new(drpc_channel_t chan) {
    drpc_session_t sess = (drpc_session_t)drpc_alloc(sizeof(*sess));
    if (!sess) {
        return NULL;
    }
    sess->channel = chan;
    sess->output.body = NULL;
    sess->input.body = NULL;
    sess->iov.iov_base = &sess->input.header;
    sess->iov.iov_len = sizeof(sess->input.header);
    sess->is_body = 0;
    return sess;
}

int drpc_session_read(drpc_session_t sess) {
    int fd = sess->channel->endpoint;
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

int drpc_session_write(drpc_session_t sess) {
    int fd = sess->channel->endpoint;
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

void drpc_session_process(drpc_session_t sess) {
    drpc_message_t rmsg = &sess->input;
    //fprintf(stderr, "seq=%u payload=%u\n", rmsg->header.sequence, rmsg->header.payload);
    //fflush(stderr);
    //write(STDERR_FILENO, rmsg->body, rmsg->header.payload);
    //write(STDERR_FILENO, "\n", 1);
    drpc_message_t smsg = &sess->output;
    smsg->header.payload = rmsg->header.payload;
    smsg->body = drpc_alloc(rmsg->header.payload); // ECHO
    memcpy(smsg->body, rmsg->body, rmsg->header.payload); // ECHO

    drpc_free(sess->input.body);
    sess->input.body = NULL;
    smsg->header.sequence = rmsg->header.sequence;
    sess->iov.iov_base = &sess->output.header;
    sess->iov.iov_len = sizeof(sess->output.header);
    sess->is_body = 0;
    STAILQ_INSERT_TAIL(&sess->channel->sessions, sess, entries);
}

void drpc_session_drop(drpc_session_t sess) {
    if (!sess) {
        return;
    }
    if (sess->output.body != NULL) {
        drpc_free(sess->output.body);
    }
    if (sess->input.body != NULL) {
        drpc_free(sess->input.body);
    }
    //DRPC_LOG(DEBUG, "free session %p", sess);
    drpc_free(sess);
}

