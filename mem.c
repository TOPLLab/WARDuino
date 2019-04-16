#include "mem.h"
#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Assert calloc
void *acalloc(size_t nmemb, size_t size, char *name)
{
    if ((int)(nmemb * size) == 0)
    {
        return NULL;
    }
    else
    {
        debug("IN Acalloc  count: %d, size: %d for %s \n", nmemb, size, name);
        void *res = calloc(nmemb, size);
        debug("Done ... Acalloc\n");
        if (res == NULL)
        {
            FATAL("Could not allocate %d bytes for %s \n", (int)(nmemb * size), name);
        }
        return res;
    }
}

// Assert realloc/calloc
void *arecalloc(void *ptr, size_t old_nmemb, size_t nmemb, size_t size, char *name)
{
    size_t *res = (size_t *)calloc(nmemb, size);
    if (res == NULL)
    {
        FATAL("Could not allocate %d bytes for %s", (int)(nmemb * size), name);
    }
    memmove(res, ptr, old_nmemb * size);
    // Initialize new memory
    memset(res + old_nmemb * size, 0, (nmemb - old_nmemb) * size);
    return res;
}
