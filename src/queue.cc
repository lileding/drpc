#include "queue.h"

namespace serpc {

int Queue::change(uintptr_t ident, int16_t filter, uint16_t flags,
        uint32_t fflags, intptr_t data, void* udata) noexcept {
    return -1;
}

int Queue::wait(struct kevent* eventlist, int nevents) noexcept {
    return -1;
}

} /* namespace serpc */

