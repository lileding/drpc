#ifndef DRPC_SRC_SESSION_H
#define DRPC_SRC_SESSION_H

#include <sys/queue.h>

struct drpc_channel;
struct drpc_message;

struct drpc_session {
    STAILQ_ENTRY(drpc_session) entries;
    struct drpc_channel* channel;
    struct drpc_message* input;
    struct drpc_message* output;
};
typedef struct drpc_session* drpc_session_t;

drpc_session_t drpc_session_new(struct drpc_channel* chan);
void drpc_session_drop(drpc_session_t sess);

void drpc_session_process(drpc_session_t sess);

#endif /* DRPC_SRC_SESSION_H */

