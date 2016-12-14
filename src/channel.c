#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/event.h>
#include "endpoint.h"
#include "channel.h"

static drpc_session_t drpc_session_new(drpc_channel_t chan);
static int drpc_session_send(drpc_session_t sess);
static int drpc_session_receive(drpc_session_t sess);
static void drpc_session_process(drpc_session_t sess);
static void drpc_session_drop(drpc_session_t sess);

drpc_channel_t drpc_channel_new(int fd) {
    DRPC_ENSURE_OR(fd != -1, NULL, "invalid argument");
    if (drpc_set_nonblock(fd) == -1) {
        DRPC_LOG(ERROR, "set nonblock fail: %s", strerror(errno));
        return NULL;
    }
    drpc_channel_t chan = (drpc_channel_t)malloc(sizeof(*chan));
    if (!chan) {
        DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
        return NULL;
    }
    chan->endpoint = fd;
    chan->receive = drpc_session_new(chan);
    if (!chan->receive) {
        DRPC_LOG(ERROR, "create session fail: %s", strerror(errno));
        drpc_channel_drop(chan);
        return NULL;
    }
    STAILQ_INIT(&chan->send_queue);
    chan->can_receive = 1;
    chan->can_send = 1;
    return chan;
}

void drpc_channel_drop(drpc_channel_t chan) {
    if (!chan) {
        return;
    }
    drpc_session_drop(chan->receive);
    while (!STAILQ_EMPTY(&chan->send_queue)) {
        drpc_session_t sess = STAILQ_FIRST(&chan->send_queue);
        STAILQ_REMOVE_HEAD(&chan->send_queue, entries);
        drpc_session_drop(sess);
    }
    close(chan->endpoint);
    DRPC_LOG(DEBUG, "free channel %p", chan);
    free(chan);
}

int drpc_channel_process(drpc_channel_t chan, int16_t filter) {
    DRPC_ENSURE_OR(chan, -1, "invalid argument");
    if (filter & EVFILT_READ) {
        DRPC_LOG(DEBUG, "read event begin");
        while (chan->can_receive) {
            int rv = drpc_session_receive(chan->receive);
            if (rv == DRPC_IO_BLOCK) {
                DRPC_LOG(DEBUG, "read block [pending=%zu]", chan->receive->iov.iov_len);
                break;
            } else if (rv == DRPC_IO_FAIL) {
                DRPC_LOG(ERROR, "channel receive fail: %s", strerror(errno));
                chan->can_receive = 0;
                if (!chan->can_send) {
                    return -1;
                }
                break;
            }
            drpc_session_process(chan->receive);
            chan->receive = drpc_session_new(chan);
            if (!chan->receive) {
                DRPC_LOG(ERROR, "create session fail: %s", strerror(errno));
                chan->can_receive = 0;
                if (!chan->can_send) {
                    return -1;
                }
                break;
            }
        }
        DRPC_LOG(DEBUG, "read event end");
    }
    if (filter & EVFILT_WRITE) {
        DRPC_LOG(DEBUG, "write event begin");
        while (chan->can_send && !STAILQ_EMPTY(&chan->send_queue)) {
            drpc_session_t sess = STAILQ_FIRST(&chan->send_queue);
            int rv = drpc_session_send(sess);
            if (rv == DRPC_IO_BLOCK) {
                DRPC_LOG(DEBUG, "write block [pending=%zu]", sess->iov.iov_len);
                break;
            } else if (rv == DRPC_IO_FAIL) {
                DRPC_LOG(ERROR, "channel send fail: %s", strerror(errno));
                chan->can_send = 0;
                if (!chan->can_receive) {
                    return -1;
                }
                break;
            }
            STAILQ_REMOVE_HEAD(&chan->send_queue, entries);
            drpc_session_drop(sess);
        }
        DRPC_LOG(DEBUG, "write event end");
    }
    return 0;
}

drpc_session_t drpc_session_new(drpc_channel_t chan) {
    drpc_session_t sess = (drpc_session_t)malloc(sizeof(*sess));
    if (!sess) {
        return NULL;
    }
    sess->channel = chan;
    sess->send.body = NULL;
    sess->receive.body = NULL;
    sess->iov.iov_base = &sess->receive.header;
    sess->iov.iov_len = sizeof(sess->receive.header);
    sess->is_body = 0;
    return sess;
}

int drpc_session_receive(drpc_session_t sess) {
    int fd = sess->channel->endpoint;
    drpc_message_t msg = &sess->receive;
    if (sess->is_body == 0) {
        int rv = drpc_recv(fd, &sess->iov);
        if (rv != DRPC_IO_COMPLETE) {
            return rv;
        }
        msg->body = (char*)malloc(msg->header.payload);
        if (!msg->body) {
            DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
            return DRPC_IO_FAIL;
        }
        sess->iov.iov_base = msg->body;
        sess->iov.iov_len = msg->header.payload;
        sess->is_body = 1;
        DRPC_LOG(DEBUG, "receive header ok");
    }
    return drpc_recv(fd, &sess->iov);
}

int drpc_session_send(drpc_session_t sess) {
    int fd = sess->channel->endpoint;
    drpc_message_t msg = &sess->send;
    DRPC_LOG(DEBUG, "sent [sock=%d] [seq=%u]", fd, msg->header.sequence);
    if (sess->is_body == 0) {
        int rv = drpc_send(fd, &sess->iov);
        if (rv != DRPC_IO_COMPLETE) {
            return rv;
        }
        sess->iov.iov_base = msg->body;
        sess->iov.iov_len = msg->header.payload;
        sess->is_body = 1;
    }
    DRPC_LOG(DEBUG, "sent [sock=%d] [seq=%u] [payload=%u]", fd, msg->header.sequence, msg->header.payload);
    return drpc_send(fd, &sess->iov);
}

void drpc_session_process(drpc_session_t sess) {
    drpc_message_t rmsg = &sess->receive;
    fprintf(stderr, "seq=%u payload=%u\n", rmsg->header.sequence, rmsg->header.payload);
    fflush(stderr);
    write(STDERR_FILENO, rmsg->body, rmsg->header.payload);
    write(STDERR_FILENO, "\n", 1);
    drpc_message_t smsg = &sess->send;
    smsg->header.payload = rmsg->header.payload;
    smsg->body = strndup(rmsg->body, rmsg->header.payload); // ECHO

    free(sess->receive.body);
    sess->receive.body = NULL;
    smsg->header.sequence = rmsg->header.sequence;
    sess->iov.iov_base = &sess->send.header;
    sess->iov.iov_len = sizeof(sess->send.header);
    sess->is_body = 0;
    STAILQ_INSERT_TAIL(&sess->channel->send_queue, sess, entries);
}

void drpc_session_drop(drpc_session_t sess) {
    if (!sess) {
        return;
    }
    if (sess->send.body != NULL) {
        DRPC_LOG(DEBUG, "free session send body %p", sess->send.body);
        free(sess->send.body);
    }
    if (sess->receive.body != NULL) {
        DRPC_LOG(DEBUG, "free session receive body %p", sess->receive.body);
        free(sess->receive.body);
    }
    DRPC_LOG(DEBUG, "free session %p", sess);
    free(sess);
}

#if 0
namespace drpc {

Channel::Channel(int sock) noexcept:
        _sock(sock),
        _send_buf(nullptr),
        _send_flag(true),
        _receiver(&_recv_buf.header, sizeof(_recv_buf.header)),
        _recv_flag(true) {
}

Channel::~Channel() noexcept {
    if (_sock != -1) {
        close(_sock);
        _sock = -1;
    }
}

void Channel::send(const Message& msg) noexcept {
    _send_queue.push(msg);
    on_send();
}

void Channel::on_event(Queue* q, struct kevent* ev) noexcept {
    if (ev->flags & EV_EOF) {
        DRPC_LOG(DEBUG, "channel eof event fd:%d", ev->ident);
        delete this;
        return;
    }
    if (ev->flags & EV_ERROR) {
        DRPC_LOG(DEBUG, "channel error event err:%s", strerror(ev->data));
        delete this;
        return;
    }
    if (ev->filter & EVFILT_READ) {
        on_recv();
    }
    if (ev->filter & EVFILT_WRITE) {
        on_send();
    }

}

void Channel::on_recv() noexcept {
    while (1) {
        auto rv = _receiver.read(_sock);
        if (rv == CONTINUE) {
            return;
        } else if (rv == ERROR) {
            DRPC_LOG(ERROR, "channel receive fail: %s", strerror(errno));
            // FIXME delete this;
            delete this;
            break;
        } else /* rv == DONE */ {
            if (_recv_flag) {
                DRPC_LOG(DEBUG, "header, seq=%lu, size=%lu",
                        _recv_buf.header.sequence, _recv_buf.header.payload);
                if (_recv_buf.header.payload == 0) {
                    _receiver.reset(&_recv_buf.header, sizeof(_recv_buf.header));
                } else {
                    _recv_buf.body = malloc(_recv_buf.header.payload);
                    _receiver.reset(_recv_buf.body, _recv_buf.header.payload);
                    _recv_flag = false;
                }
            } else {
                on_message(_recv_buf);
                free(_recv_buf.body);
                _recv_buf.body = nullptr;
                _receiver.reset(&_recv_buf.header, sizeof(_recv_buf.header));
                _recv_flag = true;
            }
        }
    }
}

void Channel::on_send() noexcept {
    while (1) {
        if (!_send_buf) {
            if (_send_queue.empty()) {
                DRPC_LOG(DEBUG, "channel send idle");
                return; // nothing to do
            } else {
                _send_buf = &_send_queue.front();
                _sender.reset(
                    &const_cast<Message*>(_send_buf)->header,
                    sizeof(_send_buf->header));
                _send_flag = true;
            }
        }
        auto rv = _sender.write(_sock);
        if (rv == CONTINUE) {
            return;
        } else if (rv == ERROR) {
            DRPC_LOG(ERROR, "channel send fail: %s", strerror(errno));
            // FIXME oops
            return;
        } else {
            if (_send_flag) {
                _sender.reset(_send_buf->body, _send_buf->header.payload);
                _send_flag = false;
            } else {
                _send_queue.pop();
                free(_send_buf->body);
                _send_buf = nullptr;
                _send_flag = true;
            }
        }
    }
}

} /* namespace drpc */
#endif

