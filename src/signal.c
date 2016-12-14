#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <drpc.h>
#include "signal.h"

int drpc_signal_open(drpc_signal_t sig) {
    DRPC_ENSURE_OR(sig, -1, "invalid argument");
    return pipe(sig->fildes);
}

void drpc_signal_close(drpc_signal_t sig) {
    if (!sig) return;
    if (sig->fildes[0] != -1) {
        close(sig->fildes[0]);
    }
    if (sig->fildes[1] != -1) {
        close(sig->fildes[1]);
    }
}

int drpc_signal_yield(drpc_signal_t sig) {
    DRPC_ENSURE_OR(sig, -1, "invalid argument");
    return sig->fildes[0];
}

int drpc_signal_notify(drpc_signal_t sig) {
    DRPC_ENSURE_OR(sig, -1, "invalid argument");
    static const char MSG[] = { '\0' };
    ssize_t len = write(sig->fildes[1], MSG, sizeof(MSG));
    return len > 0 ? 0 : -1;
}

