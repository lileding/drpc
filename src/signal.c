#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <drpc.h>
#include "mem.h"
#include "signal.h"
#include "endpoint.h"

drpc_signal_t drpc_signal_new() {
    drpc_signal_t sig = (drpc_signal_t)drpc_alloc(sizeof(*sig));
    sig->wait = -1;
    sig->notify = -1;
    if (pipe(sig->fildes) == -1) {
        DRPC_LOG(ERROR, "pipe fail: %s", strerror(errno));
        drpc_signal_drop(sig);
        return NULL;
    }
    if (drpc_set_nonblock(sig->wait) == -1) {
        DRPC_LOG(ERROR, "set nonblock fail: %s", strerror(errno));
        drpc_signal_drop(sig);
        return NULL;
    }
    return sig;
}

void drpc_signal_drop(drpc_signal_t sig) {
    if (!sig) {
        return;
    }
    if (sig->wait != -1) {
        close(sig->wait);
    }
    if (sig->notify != -1) {
        close(sig->notify);
    }
    drpc_free(sig);
}

int drpc_signal_yield(drpc_signal_t sig) {
    DRPC_ENSURE_OR(sig, -1, "invalid argument");
    return sig->wait;
}

int drpc_signal_notify(drpc_signal_t sig) {
    DRPC_ENSURE_OR(sig, -1, "invalid argument");
    static const char MSG[] = { '\0' };
    ssize_t len = write(sig->notify, MSG, sizeof(MSG));
    if (len != sizeof(MSG)) {
        DRPC_LOG(ERROR, "write pipe fail: %s", strerror(errno));
    }
    return len == sizeof(MSG) ? 0 : -1;
}

