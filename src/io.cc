#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "io.h"

namespace serpc {

IOStatus IOJobBase::read(int fd) noexcept {
    while (_pending) {
        ssize_t len = ::read(fd, _base, _pending);
        if (len == 0) {
            return DONE;
        } else if (len == -1) {
            if (errno == EAGAIN) {
                return CONTINUE;
            } else {
                return ERROR;
            }
        } else {
            _base += len;
            _pending -= len;
        }
    }
    return DONE;
}

IOStatus IOJobBase::write(int fd) noexcept {
    while (_pending) {
        ssize_t len = ::write(fd, _base, _pending);
        if (len == 0) {
            return DONE;
        } else if (len == -1) {
            if (errno == EAGAIN) {
                return CONTINUE;
            } else {
                return ERROR;
            }
        } else {
            _base += len;
            _pending -= len;
        }
    }
    return DONE;
}

IOJob<char>::~IOJob() noexcept {
    if (_buf) {
        free(_buf);
        _sz = 0;
    }
}

void IOJob<char>::reset(size_t sz) noexcept {
    _buf = reinterpret_cast<char*>(realloc(_buf, sz));
    _sz = sz;
    IOJobBase::reset(_buf, _sz);
}

} /* namespace serpc */

