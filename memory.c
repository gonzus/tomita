#include <errno.h>
#include <stdlib.h>
#include "log.h"
#include "memory.h"

void* memory_realloc(void* ptr, unsigned len) {
    if (len <= 0) {
        // want to release the memory
        if (ptr) {
            free(ptr);
        }
        return 0;
    }

    // want to create / resize the memory
    void* tmp = (void*) realloc(ptr, len);
    if (tmp) {
        return tmp;
    }

    // bad things happened
    if (errno) {
        LOG_WARN("Could not realloc %p to %lu bytes", ptr, len);
    } else {
        LOG_WARN("Could not allocate memory to realloc %p to %lu bytes", ptr, len);
    }
    abort();
    return 0;
}
