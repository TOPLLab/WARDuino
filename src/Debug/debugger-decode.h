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
    switch (static_cast<debug_Command>(type)) {
        case debug_Command_COMMAND_RUN:
        case debug_Command_COMMAND_HALT:
        case debug_Command_COMMAND_PAUSE:
        case debug_Command_COMMAND_STEP:
        case debug_Command_COMMAND_STEP_OVER:
        case debug_Command_COMMAND_ADD_BREAKPOINT:
        case debug_Command_COMMAND_REMOVE_BREAKPOINT:
        case debug_Command_COMMAND_CLEAR_BREAKPOINTS:
        case debug_Command_COMMAND_HEAP_USAGE:
        case debug_Command_COMMAND_SNAPSHOT:
        case debug_Command_COMMAND_UPDATE_FUNCTION:
        case debug_Command_COMMAND_UPDATE_LOCAL:
        case debug_Command_COMMAND_UPDATE_CALLBACKS:
        case debug_Command_COMMAND_LOAD_SNAPSHOT:
        case debug_Command_COMMAND_PROXIFY:
        case debug_Command_COMMAND_ADD_PROXY:
        case debug_Command_COMMAND_REMOVE_PROXY:
        case debug_Command_COMMAND_PROXY_CALL:
        case debug_Command_COMMAND_POP_EVENT:
        case debug_Command_COMMAND_PUSH_EVENT:
        case debug_Command_COMMAND_CONTINUE_FOR:
        case debug_Command_COMMAND_RESET:
        case debug_Command_COMMAND_INVOKE:
        case debug_Command_COMMAND_UPDATE_MODULE:
        case debug_Command_COMMAND_UPDATE_GLOBAL:
        case debug_Command_COMMAND_UPDATE_STACK:
        case debug_Command_COMMAND_SET_SNAPSHOT_POLICY:
        case debug_Command_COMMAND_SET_OVERRIDE:
        case debug_Command_COMMAND_REMOVE_OVERRIDE:
            return true;
        default:
            return false;
    }
}

template <typename T>
bool decode_payload(const std::vector<uint8_t> &payload,
                    const pb_msgdesc_t *fields, T *message) {
    pb_istream_t stream =
        pb_istream_from_buffer(payload.data(), payload.size());
    return pb_decode(&stream, fields, message);
}

// Nanopb delivers callback fields incrementally. Keep the decoded data owned
// until the command has been completely validated.
bool collect_bytes(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<uint8_t> *>(*arg);
    out->resize(stream->bytes_left);
    return out->empty() || pb_read(stream, out->data(), out->size());
}

void set_decode_callback(pb_callback_t *callback, std::vector<uint8_t> *out) {
    callback->funcs.decode = collect_bytes;
    callback->arg = out;
}

bool collect_varints(pb_istream_t *stream, const pb_field_iter_t *,
                     void **arg) {
    auto *out = static_cast<std::vector<uint32_t> *>(*arg);
    while (stream->bytes_left != 0) {
        uint64_t value = 0;
        if (!pb_decode_varint(stream, &value) || value > UINT32_MAX)
            return false;
        out->push_back(static_cast<uint32_t>(value));
    }
    return true;
}

bool collect_words(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<uint32_t> *>(*arg);
    while (stream->bytes_left != 0) {
        uint32_t value = 0;
        if (!pb_decode_fixed32(stream, &value)) return false;
        out->push_back(value);
    }
    return true;
}

bool collect_values(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<debug_Value> *>(*arg);
    debug_Value value = debug_Value_init_zero;
    if (!pb_decode(stream, debug_Value_fields, &value)) return false;
    out->push_back(value);
    return true;
}

// Shared conversion for value-update commands and snapshot restoration.
bool assign_value(const debug_Value &from, StackValue *to) {
    switch (from.which_data) {
        case debug_Value_i32_bits_tag:
            to->value_type = I32;
            to->value.uint32 = from.data.i32_bits;
            return true;
        case debug_Value_i64_bits_tag:
            to->value_type = I64;
            to->value.uint64 = from.data.i64_bits;
            return true;
        case debug_Value_f32_bits_tag:
            to->value_type = F32;
            to->value.uint32 = from.data.f32_bits;
            return true;
        case debug_Value_f64_bits_tag:
            to->value_type = F64;
            to->value.uint64 = from.data.f64_bits;
            return true;
        default:
            return false;
    }
}

struct DecodedCallbackEntry {
    std::string topic;
    std::vector<uint32_t> indexes;
};

bool collect_callback_entries(pb_istream_t *stream, const pb_field_iter_t *,
                              void **arg) {
    auto *entries = static_cast<std::vector<DecodedCallbackEntry> *>(*arg);
    debug_CallbackEntry entry = debug_CallbackEntry_init_zero;
    std::vector<uint8_t> topic;
    std::vector<uint32_t> indexes;
    set_decode_callback(&entry.topic, &topic);
    entry.table_indexes.funcs.decode = collect_varints;
    entry.table_indexes.arg = &indexes;
    if (!pb_decode(stream, debug_CallbackEntry_fields, &entry)) return false;
    entries->push_back(
        {std::string(topic.begin(), topic.end()), std::move(indexes)});
    return true;
}

std::optional<uint32_t> find_imported_function(Module *m,
                                               const std::string &name) {
    for (uint32_t index = 0; index < m->import_count; ++index) {
        if (m->functions[index].import_field != nullptr &&
            name == m->functions[index].import_field)
            return index;
    }
    return std::nullopt;
}

}  // namespace
