#ifndef MEM_H
#define MEM_H

#include <cstdio>

void *acalloc(size_t nmemb, size_t size, const char *name);

void *arealloc(void *ptr, size_t nmemb, size_t size, const char *name);

#endif 
