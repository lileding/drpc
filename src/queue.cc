#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "queue.h"

namespace serpc {

int Queue::change(uintptr_t ident, int16_t filter, uint16_t flags,
        uint32_t fflags, intptr_t data, void* udata) noexcept {
    struct kevent ev;
    EV_SET(&ev, ident, filter, flags, fflags, data, udata);
    return kevent(_kq, &ev, 1, nullptr, 0, nullptr);
}

int Queue::wait(struct kevent* eventlist, int nevents) noexcept {
    return -1;
}

} /* namespace serpc */

