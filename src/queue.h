#ifndef SERPC_SRC_QUEUE_H
#define SERPC_SRC_QUEUE_H

#include <stdint.h>

namespace serpc {

class Queue {
public:
    inline explicit Queue(int kq) noexcept: _kq(kq) { }
    inline Queue(Queue&& rhs) noexcept: _kq(rhs._kq) { rhs._kq = -1; }
    inline Queue& operator=(Queue&& rhs) noexcept {
        _kq = rhs._kq;
        rhs._kq = -1;
        return *this;
    }
public:
    inline operator int() noexcept { return _kq; }
    int change(uintptr_t ident, int16_t filter, uint16_t flags,
            uint32_t fflags, intptr_t data, void* udata) noexcept;
    int wait(struct kevent* eventlist, int nevents) noexcept;
private:
    int _kq;
};

} /* namespace serpc */

#endif /* SERPC_SRC_QUEUE_H */

