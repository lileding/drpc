#if defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <drpc.h>
#include "mem.h"
#include "event.h"

struct drpc_select {
    int kq;
    struct kevent evs[DRPC_EVENT_LIMIT];
};

drpc_select_t drpc_select_new() {
    drpc_select_t sel = drpc_new(drpc_select);
    sel->kq = kqueue();
    if (sel->kq == -1) {
        DRPC_LOG(ERROR, "kevent fail: %s", strerror(errno));
        drpc_free(sel);
        return NULL;
    }
    return sel;
}

void drpc_select_drop(drpc_select_t sel) {
    DRPC_ENSURE(sel, "invalid argument");
    close(sel->kq);
    drpc_free(sel);
}

int drpc_select_add(drpc_select_t sel, drpc_event_t ev) {
    DRPC_ENSURE(sel && ev, "invalid argument");
    struct kevent kev;
    int16_t ev_filter = 0;
    uint16_t ev_flags = EV_ADD;
    if (ev->__drpc_event_flags & DRPC_EVENT_READ) {
        ev_filter |= EVFILT_READ;
    }
    if (ev->__drpc_event_flags & DRPC_EVENT_WRITE) {
        ev_filter |= EVFILT_WRITE;
    }
    if (ev->__drpc_event_flags & DRPC_EVENT_EDGE) {
        ev_flags |= EV_CLEAR;
    }
    if (ev->__drpc_event_flags & DRPC_EVENT_ONCE) {
        ev_flags |= EV_ONESHOT;
    }
    EV_SET(&kev, ev->__drpc_event_ident, ev_filter, ev_flags, 0, 0, ev);
    return kevent(sel->kq, &kev, 1, NULL, 0, NULL);
}

int drpc_select_del(drpc_select_t sel, drpc_event_t ev) {
    DRPC_ENSURE(sel && ev, "invalid argument");
    struct kevent kev;
    int16_t ev_filter = 0;
    if (ev->__drpc_event_flags & DRPC_EVENT_READ) {
        ev_filter |= EVFILT_READ;
    }
    if (ev->__drpc_event_flags & DRPC_EVENT_WRITE) {
        ev_filter |= EVFILT_WRITE;
    }
    EV_SET(&kev, ev->__drpc_event_ident, ev_filter, EV_DELETE, 0, 0, NULL);
    return kevent(sel->kq, &kev, 1, NULL, 0, NULL);
}

int drpc_select_wait(drpc_select_t sel, struct timespec* timeout) {
    DRPC_ENSURE(sel, "invalid argument");
    int rv = kevent(sel->kq, NULL, 0, sel->evs, DRPC_EVENT_LIMIT, timeout);
    if (rv < 0) {
        DRPC_LOG(ERROR, "kevent wait fail: %s", strerror(errno));
        return rv;
    }
    for (int i = 0; i < rv; i++) {
        struct kevent* kev = sel->evs + i;
        uint16_t flags = 0x0;
        if (kev->filter & EVFILT_READ) {
            flags |= DRPC_EVENT_READ;
        }
        if (kev->filter & EVFILT_WRITE) {
            flags |= DRPC_EVENT_WRITE;
        }
        drpc_event_t ev = (drpc_event_t)kev->udata;
        ev->__drpc_event_func(ev, flags);
    }
    return 0;
}

#endif /* BSD based */

