#ifndef DRPC_SRC_SIGNAL_H
#define DRPC_SRC_SIGNAL_H

struct drpc_signal {
    union {
        int fildes[2];
        struct {
            int wait;
            int notify;
        };
    };
};

typedef struct drpc_signal* drpc_signal_t;

drpc_signal_t drpc_signal_new();

void drpc_signal_drop(drpc_signal_t sig);

int drpc_signal_yield(drpc_signal_t sig);

int drpc_signal_notify(drpc_signal_t sig);

#endif /* DRPC_SRC_SIGNAL_H */

