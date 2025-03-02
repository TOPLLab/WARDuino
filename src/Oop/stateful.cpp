#include "stateful.h"

#include "../Utils/util.h"

SerializeData &sync_memory(Module *m, uint32_t start, uint32_t end) {
    uint8_t len = end - start;

    auto message = new SerializeData;
    message->size = len + 2;
    auto *buffer = (unsigned char *)calloc(sizeof(char), message->size);

    *buffer = start;
    *(buffer + 1) = end;
    std::memcpy(buffer + 2, m->bytes + start, len);

    auto *hexa =
        new unsigned char[message->size];  //+2 for '\n' and '0' termination
    chars_as_hexa(hexa, buffer, message->size);
    message->raw = hexa;
    free(buffer);
    return *message;
}