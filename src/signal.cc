#include <unistd.h>
#include <sys/socket.h>
#include <serpc.h>
#include "signal.h"

namespace serpc {

Signal::Signal() noexcept: _fildes{-1, -1} {
    auto rv = pipe(_fildes);
    SERPC_ENSURE(rv == 0, "create pipe fail");
    shutdown(_fildes[0], SHUT_RD);
    shutdown(_fildes[1], SHUT_WR);
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
    static const char MSG = '\0';
    if (_fildes[0] != -1) {
        write(_fildes[0], &MSG, sizeof(MSG));
    }
}

} /* namespace serpc */

