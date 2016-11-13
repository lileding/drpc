#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "io.h"

namespace serpc {

IOStatus IOJob::read(int fd) noexcept {
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

IOStatus IOJob::write(int fd) noexcept {
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

} /* namespace serpc */

