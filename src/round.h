#ifndef DRPC_SRC_ROUND_H
#define DRPC_SRC_ROUND_H

#include <sys/queue.h>
#include "thrpool.h"
#include "protocol.h"

struct drpc_session;

struct drpc_round {
    DRPC_TASK_BASE;
    STAILQ_ENTRY(drpc_round) entries;
    struct drpc_session* session;
    drpc_request_t request;
    struct drpc_response response;
};
typedef struct drpc_round* drpc_round_t;

drpc_round_t drpc_round_new(struct drpc_session* chan);
void drpc_round_drop(drpc_round_t sess);

#endif /* DRPC_SRC_ROUND_H */

