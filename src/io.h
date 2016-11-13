#ifndef SERPC_SRC_IO_H
#define SERPC_SRC_IO_H

#include <stddef.h>

namespace serpc {

enum IOStatus {
    DONE = 0,
    ERROR = 1,
    CONTINUE = 2,
};

class IOJob {
public:
    inline IOJob() noexcept: _base(nullptr), _pending(0) { }
    inline IOJob(char* base, size_t pending) noexcept:
        _base(base), _pending(pending) { }
public:
    inline void reset(char* base, size_t pending) noexcept {
        _base = base;
        _pending = pending;
    }
    IOStatus read(int fd) noexcept;
    IOStatus write(int fd) noexcept;
protected:
    char* _base;
    size_t _pending;
};

} /* namespace serpc */

#endif /* SERPC_SRC_IO_H */

