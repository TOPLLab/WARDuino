#include "state_de_serialiase.h"

bool deserialiseStackValueIntoDest(StackValue *dest, uint8_t *bytes) {
    uint8_t valtypes[] = {I32, I64, F32, F64};
    uint8_t type_index = *bytes++;
    if (type_index >= sizeof(valtypes)) return false;
    dest->value.uint64 = 0;  // init whole union to 0
    size_t qb = type_index == 0 || type_index == 2 ? 4 : 8;
    dest->value_type = valtypes[type_index];
    memcpy(&dest->value, bytes, qb);
    return true;
}
