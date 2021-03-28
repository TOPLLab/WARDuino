#include "mem.h"

#include <cstdlib>
#include <cstring>

#include "debug.h"

// Assert calloc
void *acalloc(size_t nmemb, size_t size, const char *name) {
    if ((int)(nmemb * size) == 0) {
        return nullptr;
    } else {
        debug("IN Acalloc  count: %zu, size: %zu for %s \n", nmemb, size, name);
        void *res = calloc(nmemb, size);
        debug("Done ... Acalloc\n");
        if (res == nullptr) {
            FATAL("Could not allocate %d bytes for %s \n", (int)(nmemb * size),
                  name);
        }
        return res;
    }
}

// Assert realloc/calloc
void *arecalloc(void *ptr, size_t old_nmemb, size_t nmemb, size_t size,
                const char *name) {
    auto *res = (size_t *)calloc(nmemb, size);
    if (res == nullptr) {
        FATAL("Could not allocate %d bytes for %s", (int)(nmemb * size), name);
    }
    memmove(res, ptr, old_nmemb * size);
    // Initialize new memory
    memset(res + old_nmemb * size, 0, (nmemb - old_nmemb) * size);
    return res;
}
