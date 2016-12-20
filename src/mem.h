#ifndef DRPC_SRC_MEM_H
#define DRPC_SRC_MEM_H

#include <stddef.h>

void* drpc_alloc(size_t len);

void* drpc_alloc2(const char* type, size_t len);
#define drpc_new(type) (struct type*)drpc_alloc2(#type, sizeof(struct type))

void drpc_free(void* ptr);

#endif /* DRPC_SRC_MEM_H */

