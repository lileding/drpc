#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <execinfo.h>
#include <sys/queue.h>
#include <drpc.h>
#include "mem.h"

#ifdef DRPC_DEBUG
#define DRPC_BACKTRACE() \
    do { \
        void *__bt_buf[100]; \
        int __bt_cnt = backtrace(__bt_buf, 100); \
        char** __bt_syms = backtrace_symbols(__bt_buf, __bt_cnt); \
        if (__bt_syms == NULL) { \
            DRPC_LOG(ERROR, "backtrace_symbols fail: %s", strerror(errno)); \
        } \
        for (int __bt_i = 0; __bt_i < __bt_cnt; __bt_i++) { \
            fprintf(stderr, "BACKTRACE %s\n", __bt_syms[__bt_i]); \
        } \
        free(__bt_syms); \
    } while (0)

struct drpc_mem {
    TAILQ_ENTRY(drpc_mem) entries;
    const char* type;
    size_t len;
    unsigned released:1;
};
typedef struct drpc_mem* drpc_mem_t;

static TAILQ_HEAD(, drpc_mem) g_mem = TAILQ_HEAD_INITIALIZER(g_mem);
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;

__attribute__((destructor))
static void drpc_check() {
    drpc_mem_t mem = NULL;
    TAILQ_FOREACH(mem, &g_mem, entries) {
        DRPC_LOG(ERROR, "LEAK %p [type=%s] [size=%zu]", mem, mem->type, mem->len);
    }
}
#endif /* DRPC_DEBUG */

void* drpc_alloc2(const char* type, size_t len) {
#ifdef DRPC_DEBUG
    size_t rlen = sizeof(struct drpc_mem) + len;
    drpc_mem_t ptr = (drpc_mem_t)malloc(rlen);
    if (!ptr) {
        DRPC_LOG(FATAL, "malloc fail: %s", strerror(errno));
        return NULL;
    }
    ptr->type = type;
    ptr->len = len;
    pthread_mutex_lock(&g_mtx);
    TAILQ_INSERT_TAIL(&g_mem, ptr, entries);
    ptr->released = 0;
    pthread_mutex_unlock(&g_mtx);
    return (void*)ptr + sizeof(struct drpc_mem);
#else
    return malloc(len);
#endif
}

void* drpc_alloc(size_t len) {
    return drpc_alloc2("unknown", len);
}

void drpc_free(void* ptr) {
#ifdef DRPC_DEBUG
    drpc_mem_t mem = (drpc_mem_t)(ptr - sizeof(struct drpc_mem));
    DRPC_LOG(DEBUG, "free %p [type=%s] [size=%zu]", ptr, mem->type, mem->len);
    pthread_mutex_lock(&g_mtx);
    if (mem->released == 1) {
        DRPC_LOG(ERROR, "double free");
        DRPC_BACKTRACE();
        abort();
    } else {
        TAILQ_REMOVE(&g_mem, mem, entries);
        mem->released = 1;
    }
    pthread_mutex_unlock(&g_mtx);
#else
    free(ptr);
#endif
}

