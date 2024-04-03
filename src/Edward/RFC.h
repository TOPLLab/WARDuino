#pragma once

#include <cstdint>

#include "../WARDuino/internals.h"
struct StackValue;
struct Type;

struct SerializeData {
    const unsigned char *raw;
    uint32_t size;
};

class RFC {
   public:
    Module *m;
    const uint32_t fidx;
    StackValue *args;
    const Type *type;
    StackValue *result;

    bool success = true;
    char *exception;
    uint16_t exception_size;

    RFC(Module *m, uint32_t id, Type *t_type, StackValue *t_args = nullptr);
};
