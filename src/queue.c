#include <drpc.h>
#include "protocol.h"
#include "queue.h"

#if 0
void drpc_queue_open(drpc_queue_t q) {
    DRPC_ENSURE(q != NULL, "invalid argument");
    STAILQ_INIT(&q->queue);
    q->current = NULL;
    q->iov = { NULL, 0 };
    q->is_body = 0;
}

void drpc_queue_close(drpc_queue_t q) {
    if (!q) {
        return;
    }
    DRPC_LOG(WARNING, "QUEUE close"); // TODO
}

int drpc_queue_read(drpc_queue_t q, int fd) {
    DRPC_ENSURE_OR(q != NULL, DRPC_IO_FAIL, "invalid argument");
    struct iovec* v = &q->iov;
    drpc_message_t m = q->current;
    int rv = DRPC_IO_FAIL;
    while (1) {
        if (m) {
            if (q->is_body == 0) {
                rv = drpc_read(fd, v);
                if (rv != DRPC_IO_COMPLETE) {
                    return rv;
                }
                m->body = (char*)drpc_alloc(m->header.payload);
                if (!m->body) {
                    DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
                    return DRPC_IO_FAIL;
                }
                v->iov_base = m->body;
                v->iov_len = m->header.payload;
                q->input_is_body = 1;
            }
            rv = drpc_read(fd, v);
            if (rv != DRPC_IO_COMPLETE) {
                return rv;
            }
            STAILQ_INSERT_TAIL(&q->queue, m, entries);
        }
        m = (drpc_message_t)drpc_alloc(sizeof(struct drpc_message));
        if (!m) {
            DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
            return DRPC_IO_FAIL;
        }
        q->current = m;
        v->iov_base = &m->header;
        v->iov_len = sizeof(m->header);
        q->is_body = 0;
    }
    return rv;
}

int drpc_queue_pop(drpc_queue_t q, drpc_message_t** ptr) {
    DRPC_ENSURE_OR(q != NULL && ptr != NULL, -1, "invalid argument");
    if (STAILQ_EMPTY(&q->queue)) {
        return 0;
    }
    drpc_message_t* m = STAILQ_FIRST(&q->queue);
    STAILQ_REMOVE_HEAD(&q->queue, entries);
    *ptr = m;
    return 1;
}
#endif

