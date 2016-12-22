#if defined(__linux__)

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <drpc.h>
#include "mem.h"
#include "event.h"

struct drpc_select {
    int epfd;
    struct epoll_event evs[DRPC_EVENT_LIMIT];
};

drpc_select_t drpc_select_new() {
    drpc_select_t sel = drpc_new(drpc_select);
    sel->epfd = epoll_create(1);
    if (sel->epfd == -1) {
        DRPC_LOG(ERROR, "epoll_create fail: %s", strerror(errno));
        drpc_free(sel);
        return NULL;
    }
    return sel;
}

void drpc_select_drop(drpc_select_t sel) {
    DRPC_ENSURE(sel, "invalid argument");
    close(sel->epfd);
    drpc_free(sel);
}

int drpc_select_add(drpc_select_t sel, drpc_event_t ev) {
    DRPC_ENSURE(sel && ev, "invalid argument");
    struct epoll_event eev;
    uint32_t eflags = 0x0;
    if (ev->__drpc_event_flags & DRPC_EVENT_READ) {
        eflags |= EPOLLIN;
    }
    if (ev->__drpc_event_flags & DRPC_EVENT_WRITE) {
        eflags |= EPOLLOUT;
    }
    if (ev->__drpc_event_flags & DRPC_EVENT_EDGE) {
        eflags |= EPOLLET;
    }
    if (ev->__drpc_event_flags & DRPC_EVENT_ONCE) {
        eflags |= EPOLLONESHOT;
    }
    eev.events = eflags;
    eev.data.ptr = ev;
    return epoll_ctl(sel->epfd, EPOLL_CTL_ADD, ev->__drpc_event_ident, &eev);
}

int drpc_select_del(drpc_select_t sel, drpc_event_t ev) {
    DRPC_ENSURE(sel && ev, "invalid argument");
    return epoll_ctl(sel->epfd, EPOLL_CTL_DEL, ev->__drpc_event_ident, NULL);
}

int drpc_select_wait(drpc_select_t sel, struct timespec* timeout) {
    DRPC_ENSURE(sel, "invalid argument");
    int to = -1;
    if (timeout) {
        to = timeout->tv_sec * 1000 + timeout->tv_nsec / 1000;
    }
    int rv = epoll_wait(sel->epfd, sel->evs, DRPC_EVENT_LIMIT, to);
    if (rv < 0) {
        DRPC_LOG(ERROR, "epoll_wait fail: %s", strerror(errno));
        return rv;
    }
    for (int i = 0; i < rv; i++) {
        struct epoll_event* eev = sel->evs + i;
        uint16_t flags = 0x0;
        if (eev->events & EPOLLIN) {
            flags |= DRPC_EVENT_READ;
        }
        if (eev->events & EPOLLOUT) {
            flags |= DRPC_EVENT_WRITE;
        }
        drpc_event_t ev = (drpc_event_t)eev->data.ptr;
        ev->__drpc_event_func(ev, flags);
    }
    return 0;
}

#endif /* Linux */

