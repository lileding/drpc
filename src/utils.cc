#include <fcntl.h>
#include "utils.h"

namespace serpc {

int set_nonblock(int fd) noexcept {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

} /* namespace serpc */

