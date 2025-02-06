#include "RFC.h"

RFC::RFC(Module *m, uint32_t id, Type *t_type, StackValue *t_args)
    : m(m), fidx(id), args(t_args), type(t_type) {}
