#include "stateful.h"

#include "../Utils/util.h"

char *sync_memory(Module *m, uint32_t start, uint32_t end) {
    uint8_t len = end - start;
    auto *buffer = (char *)calloc(sizeof(char), (2 * len) + 8);

    sprintf(buffer, "%02" PRIx8 "%02" PRIx8 "%02" PRIx8, memoryState, start, end);
    slebf(buffer + 6, m->bytes + start, len, "\n");

    return buffer;
}