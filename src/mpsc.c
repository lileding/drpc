#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/queue.h>
#include <drpc.h>
#include "mem.h"
#include "io.h"
#include "task.h"
#include "event.h"
#include "mpsc.h"

struct drpc_mpsc {
    DRPC_EVENT_BASE(rx);
    int tx;
    STAILQ_HEAD(, drpc_task) tasks;
    pthread_mutex_t mutex;
};
typedef struct drpc_mpsc* drpc_mpsc_t;

static void on_event(drpc_mpsc_t mpsc, uint16_t flags);

drpc_mpsc_t drpc_mpsc_new() {
    int fds[2] = { 0, 0 };
    if (pipe(fds) != 0) {
        DRPC_LOG(ERROR, "pipe fail: %s", strerror(errno));
        return NULL;
    }
    drpc_mpsc_t mpsc = drpc_new(drpc_mpsc);
    mpsc->rx = fds[0];
    mpsc->tx = fds[1];
    if (drpc_set_nonblock(mpsc->rx) != 0) {
        goto FAILURE;
    }
    DRPC_EVENT_INIT(mpsc, mpsc->rx, on_event, DRPC_EVENT_READ | DRPC_EVENT_EDGE);
    int err = pthread_mutex_init(&mpsc->mutex, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(err));
        goto FAILURE;
    }
    STAILQ_INIT(&mpsc->tasks);
    return mpsc;
FAILURE:
    close(mpsc->rx);
    close(mpsc->tx);
    drpc_free(mpsc);
    return NULL;
}

void drpc_mpsc_drop(drpc_mpsc_t mpsc) {
    DRPC_ENSURE(mpsc, "invalid argument");
    pthread_mutex_destroy(&mpsc->mutex);
    close(mpsc->rx);
    close(mpsc->tx);
    drpc_free(mpsc);
}

int drpc_mpsc_send(drpc_mpsc_t mpsc, drpc_task_t task) {
    DRPC_ENSURE(mpsc, "invalid argument");
    if (!task) {
        DRPC_LOG(ERROR, "invalid argument");
        return -1;
    }
    pthread_mutex_lock(&mpsc->mutex);
    if (STAILQ_EMPTY(&mpsc->tasks)) {
        ssize_t rv = write(mpsc->tx, "", 1);
        if (rv != 1) {
            DRPC_LOG(ERROR, "pipe write fail: %s", strerror(errno));
            pthread_mutex_unlock(&mpsc->mutex);
            return -1;
        }
    }
    STAILQ_INSERT_TAIL(&mpsc->tasks, task, __drpc_task_entries);
    pthread_mutex_unlock(&mpsc->mutex);
    return 0;
}

void on_event(drpc_mpsc_t mpsc, uint16_t flags) {
    char ch = '\0';
    ssize_t rv = read(mpsc->rx, &ch, 1);
    if (rv != 1) {
        DRPC_LOG(ERROR, "pipe read fail: %s", strerror(errno));
        return;
    }
    drpc_task_t task = NULL;
    while (1) {
        pthread_mutex_lock(&mpsc->mutex);
        task = STAILQ_FIRST(&mpsc->tasks);
        if (!task) {
            pthread_mutex_unlock(&mpsc->mutex);
            return;
        }
        STAILQ_REMOVE_HEAD(&mpsc->tasks, __drpc_task_entries);
        pthread_mutex_unlock(&mpsc->mutex);
        task->__drpc_task_func(task);
    }
}

