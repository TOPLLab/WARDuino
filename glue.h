#ifndef WARDUINO_GLUE_H
#define WARDUINO_GLUE_H

#include <string>
#include <cstdint>

std::string parse_ts_string(const uint8_t *buffer, uint32_t size, uint32_t offset);

std::string parse_rust_string(const uint8_t *buffer, uint32_t size, uint32_t offset);

#endif