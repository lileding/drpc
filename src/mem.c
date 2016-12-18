#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <drpc.h>
#include "mem.h"

static void* base = NULL;
static const size_t size = 4UL * 1024 * 1024 * 1024;
static size_t offset = 0;

void* drpc_alloc(size_t len) {
    return malloc(len); // FIXME
    if (base == NULL) {
        base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
        if (base == MAP_FAILED) {
            DRPC_LOG(FATAL, "create memory pool fail: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    //void* ptr = malloc(len);
    if (offset + len > size) {
        DRPC_LOG(FATAL, "memory full");
        return NULL;
    }
    void* ptr = (char*)base + offset;
    offset += len;
    return ptr;
}

void drpc_free(void* ptr) {
    return; // FIXME
    if (ptr < base || ptr > base + size) {
        DRPC_LOG(FATAL, "free invalid ptr [base=%p] [limit=%p] [ptr=%p]",
            base, (char*)base + size, ptr
        );
        exit(EXIT_FAILURE);
    }
    // free(ptr);
}

