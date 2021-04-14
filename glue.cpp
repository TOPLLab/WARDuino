#include "glue.h"

std::string parse_utf8_string(const uint8_t *buffer, uint32_t size,
                              uint32_t offset) {
    std::string str;
    size += offset;
    while (offset < size) {
        str += buffer[offset];
        offset++;
    }
    return str;
}