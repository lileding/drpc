#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <drpc.h>

const char* drpc_now(char* buf, size_t bufsz) {
    struct timeval tv = { 0, 0 };
    gettimeofday(&tv, NULL);
    struct tm ltm;
    size_t len = strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S",
            localtime_r(&tv.tv_sec, &ltm));
    if (len <= bufsz) {
        snprintf(buf + len, bufsz - len, ".%06d", tv.tv_usec);
    }
    return buf;
}

static int drpc_loglevel = DRPC_LOGLEVEL_WARNING;

int drpc_log(int level, const char* fmt, ...) {
    if (level < drpc_loglevel) {
        return 0;
    }
    va_list vg;
    va_start(vg, fmt);
    int rv = vfprintf(stderr, fmt, vg);
    va_end(vg);
    return rv;
}

int drpc_set_loglevel(int level) {
    int old = drpc_loglevel;
    if (level >= DRPC_LOGLEVEL_DEBUG && level <= DRPC_LOGLEVEL_FATAL) {
        drpc_loglevel = level;
    }
    return old;
}

