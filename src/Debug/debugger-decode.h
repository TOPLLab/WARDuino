#pragma once

#include "debugger-private.h"

#pragma GCC diagnostic ignored "-Wunused-function"

namespace {

bool decode_frame_length(const std::vector<uint8_t> &bytes, size_t *headerSize,
                         size_t *payloadSize) {
    if (bytes.size() < 2) return false;
    uint32_t value = 0;
    for (size_t i = 0; i < 5; ++i) {
        const size_t offset = i + 1;
        if (offset >= bytes.size()) return false;
        const uint8_t byte = bytes[offset];
        if (i == 4 && (byte & 0xf0U) != 0) {
            *headerSize = SIZE_MAX;
            return false;
        }
        value |= static_cast<uint32_t>(byte & 0x7fU) << (i * 7U);
        if ((byte & 0x80U) == 0) {
            if (i > 0 && value < (1U << (i * 7U))) {
                *headerSize = SIZE_MAX;
                return false;
            }
            *headerSize = offset + 1;
            *payloadSize = value;
            return true;
        }
    }
    return false;
}

bool is_known_command(const uint8_t type) {
    return type <= static_cast<uint8_t>(debug_Command_COMMAND_REMOVE_OVERRIDE);
}

template <typename T>
bool decode_payload(const std::vector<uint8_t> &payload,
                    const pb_msgdesc_t *fields, T *message) {
    pb_istream_t stream =
        pb_istream_from_buffer(payload.data(), payload.size());
    return pb_decode(&stream, fields, message);
}

}  // namespace
