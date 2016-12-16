#ifndef DRPC_SRC_THRPOOL_H
#define DRPC_SRC_THRPOOL_H

#include <stddef.h>
#include <pthread.h>
#include <sys/queue.h>

struct drpc_task {
    STAILQ_ENTRY(drpc_task) entries;
    void (*func)(void*);
    void* arg;
};
typedef struct drpc_task* drpc_task_t;

struct drpc_thrpool {
    STAILQ_HEAD(, drpc_task) tasks;
    volatile size_t actives;
    size_t token;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t* threads;
    size_t size;
    unsigned closed:1;
};
typedef struct drpc_thrpool* drpc_thrpool_t;

int drpc_thrpool_open(drpc_thrpool_t pool, size_t nthreads);
void drpc_thrpool_close(drpc_thrpool_t pool);
void drpc_thrpool_join(drpc_thrpool_t pool);
void drpc_thrpool_apply(drpc_thrpool_t pool, drpc_task_t task);

#endif /* DRPC_SRC_THRPOOL_H */

