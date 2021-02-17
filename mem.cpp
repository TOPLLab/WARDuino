#include "mem.h"
#include <cstdlib>
#include <cstring>
#include "debug.h"

// Assert calloc
void *acalloc(size_t nmemb, size_t size, const char *name) {
    if ((int) (nmemb * size) == 0) {
        return nullptr;
    } else {
        debug("IN Acalloc  count: %zu, size: %zu for %s \n", nmemb, size, name);
        void *res = calloc(nmemb, size);
        debug("Done ... Acalloc\n");
        if (res == nullptr) {
            FATAL("Could not allocate %d bytes for %s \n", (int) (nmemb * size),
                  name);
        }
        return res;
    }
}

// Assert realloc
void *arealloc(void *ptr, size_t nmemb, size_t size, const char *name) {
    auto *res = (size_t *) realloc(ptr, nmemb * size);
    if (res == nullptr) {
        FATAL("Could not allocate %d bytes for %s", (int) (nmemb * size), name);
    }
    return res;
}
