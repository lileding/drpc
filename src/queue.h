#ifndef DRPC_SRC_QUEUE_H
#define DRPC_SRC_QUEUE_H

#include <stdint.h>
#include <sys/event.h>

namespace drpc {

class Queue {
public:
    Queue() noexcept;
    ~Queue() noexcept;
    inline operator bool() noexcept { return _kq != -1; }
public:
    inline operator int() noexcept { return _kq; }
    int change(intptr_t ident,
            int16_t filter, uint16_t flags, void* udata=nullptr) noexcept;
private:
    int _kq;
};

} /* namespace drpc */

#endif /* DRPC_SRC_QUEUE_H */

