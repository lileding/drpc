#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <drpc.h>
#include "mem.h"
#include "io.h"
#include "channel.h"

int drpc_channel_open(drpc_channel_t chan) {
    DRPC_ENSURE_OR(chan, -1, "invalid argument");
    chan->ptr = NULL;
    chan->iov.iov_base = &chan->ptr;
    chan->iov.iov_len = sizeof(chan->ptr);
    chan->send = -1;
    chan->receive = -1;
    if (pipe(chan->fildes) == -1) {
        DRPC_LOG(ERROR, "pipe fail: %s", strerror(errno));
        return -1;
    }
    if (drpc_set_nonblock(chan->receive) == -1) {
        DRPC_LOG(ERROR, "set nonblock fail: %s", strerror(errno));
        close(chan->send);
        close(chan->receive);
        return -1;
    }
    return 0;
}

void drpc_channel_close(drpc_channel_t chan) {
    DRPC_ENSURE(chan != NULL, "invalid argument");
    close(chan->send);
    close(chan->receive);
}

int drpc_channel_write(drpc_channel_t chan, void* ptr) {
    DRPC_ENSURE_OR(chan, -1, "invalid argument");
    ssize_t rv = write(chan->send, &ptr, sizeof(ptr));
    if (rv != sizeof(ptr)) {
        DRPC_LOG(ERROR, "channel send fail: %s", strerror(errno));
        return -1;
    } else {
        return 0;
    }
}

void* drpc_channel_read(drpc_channel_t chan) {
    DRPC_ENSURE_OR(chan, NULL, "invalid argument");
    int rv = drpc_read(chan->receive, &chan->iov);
    if (rv == DRPC_IO_BLOCK) {
        return NULL;
    } else if (rv == DRPC_IO_FAIL) {
        DRPC_LOG(FATAL, "channel recv fail: %s", strerror(errno));
        return NULL;
    } else {
        void* ptr = chan->ptr;
        chan->iov.iov_base = &chan->ptr;
        chan->iov.iov_len = sizeof(chan->ptr);
        return ptr;
    }
}

