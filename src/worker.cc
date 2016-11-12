#if 0
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "worker.h"

namespace serpc {

void Worker::handle(Queue* q, const struct kevent* ev) noexcept {
    if (_read) {
        on_read(q, ev);
    } else {
        on_write(q, ev);
    }
}

void Worker::on_read(Queue* q, const struct kevent* ev) noexcept {
    SERPC_ENSURE(ev->filter & EVFILT_READ, "not read event");
    switch (_io.read(ev->ident)) {
    case CONTINUE:
        return;
    case ERROR:
        SERPC_LOG(WARNING, "read fail: %s", strerror(errno));
        close(ev->ident);
        return;
    case DONE:
        SERPC_LOG(DEBUG, "worker read %s", _io.data());
        if (ev->flags & EV_EOF) {
            delete reinterpret_cast<Worker*>(ev->udata);
            SERPC_LOG(DEBUG, "process client %lu done", ev->ident);
            close(ev->ident);
            return;
        }
        if (q->change(ev->ident, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, this) == -1) {
            SERPC_LOG(WARNING, "kqueue add fail: %s", strerror(errno));
            close(ev->ident);
        }
        _io.reset(_io.data(), _io.size());
        _read = false;
    }
}

void Worker::on_write(Queue* q, const struct kevent* ev) noexcept {
    SERPC_ENSURE(ev->filter & EVFILT_WRITE, "not write event");
    switch (_io.write(ev->ident)) {
    case CONTINUE:
        return;
    case ERROR:
        SERPC_LOG(WARNING, "write fail: %s", strerror(errno));
        close(ev->ident);
        return;
    case DONE:
        if (ev->flags & EV_EOF) {
            delete reinterpret_cast<Worker*>(ev->udata);
            SERPC_LOG(DEBUG, "process client %lu done", ev->ident);
            close(ev->ident);
            return;
        }
        if (q->change(ev->ident, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, this) == -1) {
            SERPC_LOG(WARNING, "kqueue add fail: %s", strerror(errno));
            close(ev->ident);
        }
        _io.reset(_io.data(), _io.size());
        _read = true;
    }
}

} /* namespace serpc */
#endif
