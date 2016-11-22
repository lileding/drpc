#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <serpc.h>
#include "channel.h"
#include "queue.h"
#include "controller.h"

namespace serpc {

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
        SERPC_LOG(DEBUG, "channel eof event fd:%d", ev->ident);
        delete this;
        return;
    }
    if (ev->flags & EV_ERROR) {
        SERPC_LOG(DEBUG, "channel error event err:%s", strerror(ev->data));
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
            SERPC_LOG(ERROR, "channel receive fail: %s", strerror(errno));
            // FIXME delete this;
            delete this;
            break;
        } else /* rv == DONE */ {
            if (_recv_flag) {
                SERPC_LOG(DEBUG, "header, seq=%lu, size=%lu",
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
                SERPC_LOG(DEBUG, "channel send idle");
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
            SERPC_LOG(ERROR, "channel send fail: %s", strerror(errno));
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

} /* namespace serpc */

