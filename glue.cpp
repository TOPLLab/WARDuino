#include "glue.h"

std::string parse_ts_string(const uint8_t *buffer, uint32_t size, uint32_t offset) {
    std::string str;
    size += offset;
    char cursor = buffer[offset];
    while(cursor != '\0' && offset < size) {
        cursor = buffer[offset];
        str += cursor;
        offset += 2;
    }
    return str;
}

std::string parse_rust_string(const uint8_t *buffer, uint32_t size, uint32_t offset) {
    std::string str;
    size += offset;
    while(offset < size) {
        str += buffer[offset];
        offset++;
    }
    return str;
}