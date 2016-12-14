#ifndef DRPC_SRC_SIGNAL_H
#define DRPC_SRC_SIGNAL_H

struct drpc_signal {
    int fildes[2];
};

typedef struct drpc_signal* drpc_signal_t;

int drpc_signal_open(drpc_signal_t sig);

void drpc_signal_close(drpc_signal_t sig);

int drpc_signal_yield(drpc_signal_t sig);

int drpc_signal_notify(drpc_signal_t sig);

#endif /* DRPC_SRC_SIGNAL_H */

