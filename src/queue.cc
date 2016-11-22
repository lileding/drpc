#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <drpc.h>
#include "queue.h"

namespace drpc {

Queue::Queue() noexcept: _kq(kqueue()) {
    DRPC_ENSURE(_kq != -1, "kqueue fail: %s", strerror(errno));
}

Queue::~Queue() noexcept {
    if (_kq != -1) {
        close(_kq);
        _kq = -1;
    }
}

int Queue::change(intptr_t ident,
        int16_t filter, uint16_t flags, void* udata) noexcept {
    struct kevent ev;
    EV_SET(&ev, ident, filter, flags, 0, 0, udata);
    return kevent(_kq, &ev, 1, nullptr, 0, nullptr);
}

} /* namespace drpc */

