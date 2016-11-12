#include <stdio.h>
#include <stdarg.h>
#include <serpc.h>

namespace serpc {

int log(const char* fmt, ...) noexcept {
    va_list vg;
    va_start(vg, fmt);
    auto rv = vfprintf(stderr, fmt, vg);
    va_end(vg);
    return rv;
}

} /* namespace serpc */

