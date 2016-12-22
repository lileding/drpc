#ifndef DRPC_SRC_EVENT_H
#define DRPC_SRC_EVENT_H

#include <stdint.h>
#include <sys/time.h>

#define DRPC_EVENT_LIMIT 1024

#define DRPC_EVENT_READ     0x0001
#define DRPC_EVENT_WRITE    0x0010
#define DRPC_EVENT_EDGE     0x0100
#define DRPC_EVENT_ONCE     0x1000

#define DRPC_EVENT_BASE(name) \
    union { \
        int __drpc_event_ident; \
        int name; \
    }; \
    void (*__drpc_event_func)(struct drpc_event*, uint16_t); \
    uint16_t __drpc_event_flags

#define DRPC_EVENT_INIT(ev, ident, func, flags) \
    do { \
        (ev)->__drpc_event_ident = (ident); \
        (ev)->__drpc_event_func = (drpc_event_func)(func); \
        (ev)->__drpc_event_flags = (flags); \
    } while (0)

struct drpc_event {
    int __drpc_event_ident;
    void (*__drpc_event_func)(struct drpc_event*, uint16_t);
    uint16_t __drpc_event_flags;

};
typedef struct drpc_event* drpc_event_t;
typedef void (*drpc_event_func)(drpc_event_t ev, uint16_t flags);

struct drpc_select;
typedef struct drpc_select* drpc_select_t;
typedef struct drpc_event* drpc_event_t;

drpc_select_t drpc_select_new();

void drpc_select_drop(drpc_select_t sel);

int drpc_select_wait(drpc_select_t sel, struct timespec* timeout);

int drpc_select_add(drpc_select_t sel, drpc_event_t ev);

int drpc_select_del(drpc_select_t sel, drpc_event_t ev);

#endif /* DRPC_SRC_EVENT_H */

