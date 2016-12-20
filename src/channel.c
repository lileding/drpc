#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <drpc.h>
#include "mem.h"
#include "io.h"
#include "channel.h"

int drpc_channel_open(drpc_channel_t chan) {
    DRPC_ENSURE(chan, "invalid argument");
    STAILQ_INIT(&chan->items);
    int err = pthread_mutex_init(&chan->mutex, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(err));
        return -1;
    }
    chan->send = -1;
    chan->receive = -1;
    if (pipe(chan->fildes) == -1) {
        DRPC_LOG(ERROR, "pipe fail: %s", strerror(errno));
        pthread_mutex_destroy(&chan->mutex);
        return -1;
    }
    if (drpc_set_nonblock(chan->receive) == -1) {
        DRPC_LOG(ERROR, "set nonblock fail: %s", strerror(errno));
        close(chan->send);
        close(chan->receive);
        pthread_mutex_destroy(&chan->mutex);
        return -1;
    }
    return 0;
}

void drpc_channel_close(drpc_channel_t chan) {
    DRPC_ENSURE(chan != NULL, "invalid argument");
    close(chan->send);
    chan->send = -1;
    close(chan->receive);
    chan->receive = -1;
    pthread_mutex_destroy(&chan->mutex);
}

void drpc_channel_write(drpc_channel_t chan, void* ptr) {
    DRPC_ENSURE(chan, "invalid argument");
    drpc_item_t item = drpc_new(drpc_item);
    item->ptr = ptr;
    int empty = 0;
    pthread_mutex_lock(&chan->mutex);
    empty = STAILQ_EMPTY(&chan->items);
    STAILQ_INSERT_TAIL(&chan->items, item, entries);
    pthread_mutex_unlock(&chan->mutex);
    if (empty && write(chan->send, "", 1) != 1) {
        DRPC_LOG(FATAL, "channel write fail: %s", strerror(errno));
    }
}

void* drpc_channel_read(drpc_channel_t chan) {
    DRPC_ENSURE(chan, "invalid argument");
    void* ptr = NULL;
    pthread_mutex_lock(&chan->mutex);
    drpc_item_t item = STAILQ_FIRST(&chan->items);
    if (item) {
        ptr = item->ptr;
        STAILQ_REMOVE_HEAD(&chan->items, entries);
        drpc_free(item);
    }
    pthread_mutex_unlock(&chan->mutex);
    return ptr;
}

int drpc_channel_wait(drpc_channel_t chan) {
    DRPC_ENSURE(chan, "invalid argument");
    char buf = '\0';
    int rv = read(chan->receive, &buf, 1) == 1 ? 0 : -1;
    if (rv != 0) {
        DRPC_LOG(FATAL, "channel read fail: %s", strerror(errno));
    }
    return rv;
}

