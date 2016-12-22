#ifndef DRPC_SRC_MPSC_H
#define DRPC_SRC_MPSC_H

struct drpc_task;

struct drpc_mpsc;
typedef struct drpc_mpsc* drpc_mpsc_t;

drpc_mpsc_t drpc_mpsc_new();

void drpc_mpsc_drop(drpc_mpsc_t mpsc);

int drpc_mpsc_send(drpc_mpsc_t mpsc, struct drpc_task* task);

#endif /* DRPC_SRC_MPSC_H */

