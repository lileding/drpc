#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <drpc.h>
#include "queue.h"

int drpc_queue_open(drpc_queue_t queue) {
    DRPC_ENSURE_OR(queue, -1, "invalid argument");
    int kq = kqueue();
    DRPC_ENSURE_OR(kq != -1, -1, "kqueue fail: %s", strerror(errno));
    queue->kq = kq;
    return 0;
}

void drpc_queue_close(drpc_queue_t queue) {
    if (!queue || queue->kq == -1) return;
    close(queue->kq);
    queue->kq = -1;
}

int drpc_queue_add(drpc_queue_t queue, intptr_t ident,
        uint32_t flags, void* data) {
    DRPC_ENSURE_OR(queue && queue->kq != -1, -1, "invalid argument");
    struct kevent ev;
    int16_t ev_filter = 0;
    uint16_t ev_flags = EV_ADD;
    if (flags & DRPC_FILT_READ) {
        ev_filter |= EVFILT_READ;
    }
    if (flags & DRPC_FILT_WRITE) {
        ev_filter |= EVFILT_WRITE;
    }
    if (flags & DRPC_EVENT_EDGE) {
        ev_flags |= EV_CLEAR;
    }
    EV_SET(&ev, ident, ev_filter, ev_flags, 0, 0, data);
    return kevent(queue->kq, &ev, 1, NULL, 0, NULL);
}

