#ifndef MEM_H
#define MEM_H

#include <cstdio>

void *acalloc(size_t nmemb, size_t size, const char *name, bool psram=false);

void *arecalloc(void *ptr, size_t old_nmemb, size_t nmemb, size_t size, const char *name, bool psram=false);

#endif 
