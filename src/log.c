#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <execinfo.h>
#include <sys/time.h>
#include <drpc.h>

#define DRPC_BACKTRACE() \
    do { \
        void *__bt_buf[100]; \
        int __bt_cnt = backtrace(__bt_buf, 100); \
        char** __bt_syms = backtrace_symbols(__bt_buf, __bt_cnt); \
        if (__bt_syms == NULL) { \
            fprintf(stderr, "backtrace_symbols fail: %s\n", strerror(errno)); \
        } \
        for (int __bt_i = 0; __bt_i < __bt_cnt; __bt_i++) { \
            fprintf(stderr, "BACKTRACE %s\n", __bt_syms[__bt_i]); \
        } \
        free(__bt_syms); \
    } while (0)

const char* drpc_now(char* buf, size_t bufsz) {
    struct timeval tv = { 0, 0 };
    gettimeofday(&tv, NULL);
    struct tm ltm;
    size_t len = strftime(buf, bufsz, "%Y-%m-%d %H:%M:%S",
            localtime_r(&tv.tv_sec, &ltm));
    if (len <= bufsz) {
        snprintf(buf + len, bufsz - len, ".%06ld", tv.tv_usec);
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
    if (level == DRPC_LOGLEVEL_FATAL) {
        DRPC_BACKTRACE();
        abort();
    }
    return rv;
}

int drpc_set_loglevel(int level) {
    int old = drpc_loglevel;
    if (level >= DRPC_LOGLEVEL_DEBUG && level <= DRPC_LOGLEVEL_FATAL) {
        drpc_loglevel = level;
    }
    return old;
}

