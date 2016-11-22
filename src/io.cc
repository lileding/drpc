#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <serpc.h>
#include "io.h"

namespace serpc {

IOStatus IOJob::read(int fd) noexcept {
    while (_nbyte) {
        ssize_t len = ::read(fd, _buf, _nbyte);
        if (len == 0) {
            return ERROR; // EOF
        } else if (len == -1) {
            if (errno == EAGAIN) {
                return CONTINUE;
            } else {
                return ERROR;
            }
        } else {
            _buf = reinterpret_cast<char*>(_buf) + len;
            _nbyte -= len;
        }
    }
    return DONE;
}

IOStatus IOJob::write(int fd) noexcept {
    while (_nbyte) {
        ssize_t len = ::write(fd, _buf, _nbyte);
        if (len == 0) {
            return ERROR; // EOF
        } else if (len == -1) {
            if (errno == EAGAIN) {
                return CONTINUE;
            } else {
                return ERROR;
            }
        } else {
            _buf = reinterpret_cast<char*>(_buf) + len;
            _nbyte -= len;
        }
    }
    return DONE;
}

} /* namespace serpc */

