#ifndef DRPC_SRC_TASK_H
#define DRPC_SRC_TASK_H

#include <stddef.h>

struct drpc_task;
typedef void (*drpc_task_func)(struct drpc_task*);

#define DRPC_TASK_BASE \
    STAILQ_ENTRY(drpc_task) __drpc_task_entries; \
    const char* __drpc_task_name; \
    void (*__drpc_task_func)(struct drpc_task*)

#define DRPC_TASK_INIT(task, name, func) \
    do { \
        (task)->__drpc_task_name = (name); \
        (task)->__drpc_task_func = (drpc_task_func)(func); \
    } while (0)

#define DRPC_VBASE(type, ptr, field) \
    (struct type*)((void*)(ptr) - offsetof(struct type, field))

struct drpc_task {
    DRPC_TASK_BASE;
};
typedef struct drpc_task* drpc_task_t;

#endif /* DRPC_SRC_TASK_H */

