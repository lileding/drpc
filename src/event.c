#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <drpc.h>
#include "event.h"

int drpc_event_open(drpc_event_t event) {
    DRPC_ENSURE(event, "invalid argument");
    int kq = kqueue();
    if (kq == -1) {
        DRPC_LOG(ERROR, "kevent fail: %s", strerror(errno));
        return -1;
    }
    event->kq = kq;
    return 0;
}

void drpc_event_close(drpc_event_t event) {
    DRPC_ENSURE(event, "invalid argument");
    close(event->kq);
    event->kq = -1;
}

int drpc_event_add(drpc_event_t event, intptr_t ident,
        uint32_t flags, void* data) {
    DRPC_ENSURE(event, "invalid argument");
    struct kevent ev;
    int16_t ev_filter = 0;
    uint16_t ev_flags = EV_ADD;
    if (flags & DRPC_EVENT_READ) {
        ev_filter |= EVFILT_READ;
    }
    if (flags & DRPC_EVENT_WRITE) {
        ev_filter |= EVFILT_WRITE;
    }
    if (flags & DRPC_EVENT_EDGE) {
        ev_flags |= EV_CLEAR;
    }
    if (flags & DRPC_EVENT_ONESHOT) {
        ev_flags |= EV_ONESHOT;
    }
    EV_SET(&ev, ident, ev_filter, ev_flags, 0, 0, data);
    return kevent(event->kq, &ev, 1, NULL, 0, NULL);
}

void drpc_event_del(drpc_event_t event, int ident) {
    DRPC_ENSURE(event, "invalid argument");
    struct kevent ev;
    EV_SET(&ev, ident, EVFILT_READ | EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(event->kq, &ev, 1, NULL, 0, NULL);
}

