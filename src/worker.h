#ifndef SERPC_SRC_WORKER_H
#define SERPC_SRC_WORKER_H

#include <serpc.h>
#include "io.h"

namespace serpc {

class Worker : public Handler {
public:
    inline explicit Worker(bool read_first) noexcept: _read(read_first) { }
public:
    void handle(Queue* q, const struct kevent* ev) noexcept override;
private:
    void on_read(Queue* q, const struct kevent* ev) noexcept;
    void on_write(Queue* q, const struct kevent* ev) noexcept;
private:
    bool _read;
    IOJob<6> _io;
};

} /* namespace serpc */

#endif /* SERPC_SRC_WORKER_H */

