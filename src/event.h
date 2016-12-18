#ifndef DRPC_SRC_EVENT_H
#define DRPC_SRC_EVENT_H

#include <stdint.h>
#include <sys/event.h>

#define DRPC_EVENT_LIMIT 1024

struct drpc_event {
    int kq;
    struct kevent evs[DRPC_EVENT_LIMIT];
};
typedef struct drpc_event* drpc_event_t;

int drpc_event_open(drpc_event_t event);

void drpc_event_close(drpc_event_t event);

#define DRPC_EVENT_READ     0x0001
#define DRPC_EVENT_WRITE    0x0010
#define DRPC_EVENT_EDGE     0x0100
#define DRPC_EVENT_ONESHOT  0x1000

int drpc_event_add(drpc_event_t event, intptr_t ident,
        uint32_t flags, void* data);

#endif /* DRPC_SRC_EVENT_H */

