#ifndef DRPC_SRC_ROUND_H
#define DRPC_SRC_ROUND_H

#include <sys/queue.h>
#include "thrpool.h"

struct drpc_session;
struct drpc_message;

struct drpc_round {
    DRPC_TASK_BASE;
    struct drpc_session* session;
    struct drpc_message* input;
    struct drpc_message* output;
};
typedef struct drpc_round* drpc_round_t;

drpc_round_t drpc_round_new(struct drpc_session* chan);
void drpc_round_drop(drpc_round_t sess);

#endif /* DRPC_SRC_ROUND_H */

