#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <serpc.h>
#include "signal.h"

namespace serpc {

Signal::Signal() noexcept: _fildes{-1, -1} {
    auto rv = pipe(_fildes);
    SERPC_ENSURE(rv == 0, "create pipe fail");
}

Signal::~Signal() noexcept {
    if (_fildes[0] != -1) {
        close(_fildes[0]);
    }
    if (_fildes[1] != -1) {
        close(_fildes[1]);
    }
}

void Signal::notify() noexcept {
    if (!*this) return;
    static const char MSG[] = { '\0' };
    auto rv = write(_fildes[1], MSG, sizeof(MSG));
    SERPC_ENSURE(rv == sizeof(MSG), "notify fail: %s", strerror(errno));
}

} /* namespace serpc */

