#include "RFC.h"

#include <cstdlib>
#include <cstring>

RFC::RFC(Module *m, uint32_t id, Type *t_type, StackValue *t_args)
    : m(m), fidx(id), args(t_args), type(t_type) {}

SerializeData *merge(SerializeData a, SerializeData b, bool divide) {
    auto *data = new SerializeData;
    auto padding = divide ? 2 : 1;
    data->size = a.size + b.size + padding;
//    size_t lengte = a.size + b.size + padding;
    data->raw = new char[data->size]; //(unsigned char *)calloc(data->size, sizeof(char));
    if (divide) {
        *(data->raw + a.size) = '\n';
        *(data->raw + a.size + b.size + 1) = '\n';
        padding = 1;
    } else {
        padding = 0;
    }
    std::memcpy(data->raw, a.raw, a.size);
    std::memcpy(data->raw + a.size + padding, b.raw, b.size);
    return data;
}

struct SerializeData* mergeSerializeData(struct SerializeData data1, struct SerializeData data2, bool divide) {
    // Allocate memory for the result struct
    struct SerializeData *result =
        static_cast<SerializeData *>(malloc(sizeof(struct SerializeData)));
    if (result == NULL) {
        return NULL; // Memory allocation failure
    }

    uint32_t extra = divide ? 1 : 0;
    result->size = data1.size + data2.size + extra;
    result->raw = static_cast<char *>(malloc(result->size));
    if (result->raw == NULL) {
        free(result);
        return NULL; // Memory allocation failure
    }

    // Copy the first data block
    memcpy(result->raw, data1.raw, data1.size);

    // Optionally add a newline character
    if (divide) {
        result->raw[data1.size] = '\n';
    }

    // Copy the second data block after the newline (if inserted)
    memcpy(result->raw + data1.size + extra, data2.raw, data2.size);

    return result;
}