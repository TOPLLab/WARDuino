#include "glue.h"

std::string parse_ts_string(const uint8_t *buffer, uint32_t size, int offset) {
    std::string str;
    char cursor = buffer[offset];
    while(cursor != '\0' && offset < size) {
        cursor = buffer[offset];
        str += cursor;
        offset += 2;
    }
    return str;
}