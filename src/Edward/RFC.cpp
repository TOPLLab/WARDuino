#include "RFC.h"

RFC::RFC(uint32_t id, Type *t_type, StackValue *t_args)
    : fidx(id), args(t_args), type(t_type) {}
