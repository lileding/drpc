#ifndef DRPC_SRC_MEM_H
#define DRPC_SRC_MEM_H

#include <stddef.h>

void* drpc_alloc(size_t len);

void drpc_free(void* ptr);

#endif /* DRPC_SRC_MEM_H */

