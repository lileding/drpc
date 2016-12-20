#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#include <dlfcn.h>
#include <drpc.h>
#include "thrpool.h"
#include "mem.h"

static void* drpc_thrpool_loop(void* arg) {
    DRPC_ENSURE(arg, "invalid argument");
    drpc_thrpool_t pool = (drpc_thrpool_t)arg;
    pthread_mutex_lock(&pool->mutex);
    //size_t token = pool->token++;
    drpc_task_t task = NULL;
    while (1) {
        while (STAILQ_EMPTY(&pool->tasks)) {
            //DRPC_LOG(DEBUG, "thrpool wait task [token=%zu] [actives=%zu]", token, pool->actives);
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }
        task = STAILQ_FIRST(&pool->tasks);
        STAILQ_REMOVE_HEAD(&pool->tasks, __drpc_task_entries);
        pool->actives--;
        //DRPC_LOG(DEBUG, "thrpool consume task [token=%zu] [actives=%zu] [empty=%d]", token, pool->actives, STAILQ_EMPTY(&pool->tasks));
        pthread_mutex_unlock(&pool->mutex);
        if (task == &pool->stop) {
            //DRPC_LOG(DEBUG, "thrpool got stop [token=%zu]", token);
            break;
        }
        //Dl_info dli;
        //dladdr(task->execute, &dli);
        //DRPC_LOG(DEBUG, "pool execute task [token=%zu] [task=%p] [execute=%p:%s]",
        //    token, task, task->execute, dli.dli_sname);
        task->__drpc_task_func(task);
        pthread_mutex_lock(&pool->mutex);
    }
    return NULL;
}

int drpc_thrpool_open(drpc_thrpool_t pool, size_t nthreads) {
    DRPC_ENSURE(pool && nthreads > 0, "invalid argument");
    STAILQ_INIT(&pool->tasks);
    pool->actives = 0;
    //pool->token = 0;
    pool->closed = 0;
    int err = pthread_mutex_init(&pool->mutex, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_mutex_init fail: %s", strerror(errno));
        drpc_thrpool_join(pool);
        return -1;
    }
    err = pthread_cond_init(&pool->cond, NULL);
    if (err != 0) {
        DRPC_LOG(ERROR, "pthread_cond_init fail: %s", strerror(errno));
        drpc_thrpool_join(pool);
        return -1;
    }
    pool->size = nthreads;
    pool->threads = (pthread_t*)drpc_alloc2("pthread_t[]", sizeof(pthread_t) * nthreads);
    if (!pool->threads) {
        DRPC_LOG(ERROR, "malloc fail: %s", strerror(errno));
        return -1;
    }
    memset(pool->threads, 0, sizeof(pthread_t) * nthreads);
    for (size_t i = 0; i < nthreads; i++) {
        err = pthread_create(pool->threads + i, NULL, drpc_thrpool_loop, pool);
        if (err != 0) {
            DRPC_LOG(ERROR, "pthread_create fail: %s", strerror(errno));
            drpc_thrpool_join(pool); // FIXME deadlock here
            return -1;
        }
    }
    return 0;
}

void drpc_thrpool_close(drpc_thrpool_t pool) {
    DRPC_ENSURE(pool, "invalid argument");
    pthread_mutex_lock(&pool->mutex);
    if (pool->closed) {
        pthread_mutex_unlock(&pool->mutex);
        return;
    }
    pool->closed = 1;
    for (size_t i = 0; i < pool->size; i++) {
        STAILQ_INSERT_TAIL(&pool->tasks, &pool->stop, __drpc_task_entries);
        pool->actives++;
    }
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}

void drpc_thrpool_join(drpc_thrpool_t pool) {
    DRPC_ENSURE(pool, "invalid argument");
    if (pool->threads) {
        for (size_t i = 0; i < pool->size; i++) {
            if (pool->threads[i] != 0) {
                pthread_join(pool->threads[i], NULL);
            }
        }
        drpc_free(pool->threads);
    }
    pthread_cond_destroy(&pool->cond);
    pthread_mutex_destroy(&pool->mutex);
}

void drpc_thrpool_apply(drpc_thrpool_t pool, drpc_task_t task) {
    DRPC_ENSURE(pool, "invalid argument");
    if (!task || !task->__drpc_task_func) {
        DRPC_LOG(ERROR, "invalid argument");
        return;
    }
    pthread_mutex_lock(&pool->mutex);
    if (pool->closed) {
        pthread_mutex_unlock(&pool->mutex);
        DRPC_LOG(WARNING, "thrpool apply but already closed");
        return;
    }
    STAILQ_INSERT_TAIL(&pool->tasks, task, __drpc_task_entries);
    pool->actives++;
    //DRPC_LOG(NOTICE, "thrpool apply [task=%p] [name=%s] [func=%p]", task, task->__drpc_task_name, task->__drpc_task_func);
    //DRPC_LOG(DEBUG, "thrpool apply %s [actives=%zu] [empty=%d]",
    //    (task == &g_stop ? "stop" : "task"), pool->actives, STAILQ_EMPTY(&pool->tasks));
    //DRPC_LOG(DEBUG, "thrpool notify one [actives=%zu]", pool->actives);
    pthread_cond_signal(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}

