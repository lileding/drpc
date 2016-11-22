#include <stdio.h>
#include <stdarg.h>
#include <drpc.h>

namespace drpc {

int log(const char* fmt, ...) noexcept {
    va_list vg;
    va_start(vg, fmt);
    auto rv = vfprintf(stderr, fmt, vg);
    va_end(vg);
    return rv;
}

} /* namespace drpc */

