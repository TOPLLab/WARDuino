#pragma once
#include <tuple>

std::tuple<uint8_t *, unsigned int, const char *> Load_WASM(
    const char *file_name);
