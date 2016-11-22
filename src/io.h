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
    inline IOJob() noexcept: _buf(nullptr), _nbyte(0) { }
    inline IOJob(void* buf, size_t nbyte) noexcept:
        _buf(buf), _nbyte(nbyte) { }
public:
    inline void reset(void* buf, size_t nbyte) noexcept {
        _buf = buf;
        _nbyte = nbyte;
    }
    IOStatus read(int fd) noexcept;
    IOStatus write(int fd) noexcept;
protected:
    void* _buf;
    size_t _nbyte;
};

} /* namespace serpc */

#endif /* SERPC_SRC_IO_H */

