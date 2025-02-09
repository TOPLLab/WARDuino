#include "RFC.h"

#include <cstdlib>
#include <cstring>

RFC::RFC(Module *m, uint32_t id, Type *t_type, StackValue *t_args)
    : m(m), fidx(id), args(t_args), type(t_type) {}

SerializeData *merge(SerializeData a, SerializeData b) {
    auto data = new SerializeData;
    data->size = a.size + b.size + 2;
    data->raw = (unsigned char *)calloc(sizeof(char), data->size);
    std::memcpy(data->raw, a.raw, a.size);
    *(data->raw + a.size) = '\n';
    std::memcpy(data->raw + a.size + 1, b.raw, b.size);
    *(data->raw + a.size + b.size + 1) = '\n';
    return data;
}
