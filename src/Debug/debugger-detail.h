#pragma once

#include "debugger-private.h"

#pragma GCC diagnostic ignored "-Wunused-function"

/**
 * Validate if there are interrupts and execute them
 *
 * The various kinds of interrupts are preceded by an identifier:
 *
 * - `0x01` : Continue running
 * - `0x02` : Halt the execution
 * - `0x03` : Pause execution
 * - `0x04` : Execute one operation and then pause
 * - `0x06` : Add a breakpoint, the address is specified as a pointer.
 *            The pointer should be specified as: 06[length][pointer]
 *            eg: 060655a5994fa3d6 (note the lack of spaces between the
 *            arguments, the 'length' is halve the size of the address string)
 * - `0x07` : Remove the breakpoint at the address specified as a pointer if it
 *            exists (see `0x06`)
 * - `0x10` : Dump information about the program
 * - `0x11` :                  show locals
 * - `0x12` : Dump full information
 * - `0x20` : Replace the content body of a function by a new function given
 *            as payload (immediately following `0x10`), see #readChange
 */
namespace {

bool collect_bytes(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<uint8_t> *>(*arg);
    out->resize(stream->bytes_left);
    return out->empty() || pb_read(stream, out->data(), out->size());
}

[[maybe_unused]] bool collect_words(pb_istream_t *stream,
                                    const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<uint32_t> *>(*arg);
    while (stream->bytes_left != 0) {
        uint32_t value = 0;
        if (!pb_decode_fixed32(stream, &value)) return false;
        out->push_back(value);
    }
    return true;
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

bool collect_values(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<debug_Value> *>(*arg);
    debug_Value value = debug_Value_init_zero;
    if (!pb_decode(stream, debug_Value_fields, &value)) return false;
    out->push_back(value);
    return true;
}

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

[[maybe_unused]] void value_to_proto(const StackValue &from,
                                     const uint32_t index, debug_Value *to) {
    *to = debug_Value_init_zero;
    to->index = index;
    switch (from.value_type) {
        case I32:
            to->which_data = debug_Value_i32_bits_tag;
            to->data.i32_bits = from.value.uint32;
            break;
        case I64:
            to->which_data = debug_Value_i64_bits_tag;
            to->data.i64_bits = from.value.uint64;
            break;
        case F32:
            to->which_data = debug_Value_f32_bits_tag;
            to->data.f32_bits = from.value.uint32;
            break;
        case F64:
            to->which_data = debug_Value_f64_bits_tag;
            to->data.f64_bits = from.value.uint64;
            break;
        default:
            break;
    }
}

using nanopb_encoder::ByteView;
using nanopb_encoder::Uint32View;

struct ValueView {
    const StackValue *values;
    size_t size;
    Global *const *globals;
};

struct EventRangeView {
    size_t begin;
    size_t size;
};

struct SnapshotView {
    Module *module;
    const Debugger *debugger;
    const ExecutionContext *context;
    const std::unordered_map<std::vector<uint32_t>, uint32_t, FNV1aVectorHash>
        *overrides;
};

ValueView current_locals(const ExecutionContext *context) {
    if (context->csp < 0 || context->fp < 0) return {nullptr, 0, nullptr};

    for (int index = context->csp; index >= 0; --index) {
        const Block *block = context->callstack[index].block;
        if (block == nullptr || block->block_type != 0 ||
            block->type == nullptr) {
            continue;
        }

        const size_t count = block->type->param_count + block->local_count;
        if (count == 0 ||
            context->fp + static_cast<int>(count) > context->sp + 1) {
            return {nullptr, 0, nullptr};
        }
        return {context->stack + context->fp, count, nullptr};
    }

    return {nullptr, 0, nullptr};
}

bool encode_value(pb_ostream_t *stream, const pb_field_t *field,
                  const StackValue &source, const size_t index) {
    debug_Value value = debug_Value_init_zero;
    value_to_proto(source, static_cast<uint32_t>(index), &value);
    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_submessage(stream, debug_Value_fields, &value);
}

bool encode_value_range(pb_ostream_t *stream, const pb_field_t *field,
                        void *const *arg) {
    const auto *view = static_cast<const ValueView *>(*arg);
    for (size_t index = 0; index < view->size; ++index) {
        const StackValue *value = view->globals == nullptr
                                      ? &view->values[index]
                                      : view->globals[index]->value;
        if (!encode_value(stream, field, *value, index)) return false;
    }
    return true;
}

bool encode_values(pb_ostream_t *stream, const pb_field_t *field,
                   void *const *arg) {
    const auto *values = static_cast<const std::vector<StackValue> *>(*arg);
    ValueView view{values->data(), values->size(), nullptr};
    void *range = &view;
    return encode_value_range(stream, field, &range);
}

bool encode_bytes(pb_ostream_t *stream, const pb_field_t *field,
                  void *const *arg) {
    const auto *bytes = static_cast<const std::vector<uint8_t> *>(*arg);
    ByteView view{bytes->data(), bytes->size()};
    void *opaque = &view;
    return nanopb_encoder::encode_bytes(stream, field, &opaque);
}

bool encode_breakpoints(pb_ostream_t *stream, const pb_field_t *field,
                        void *const *arg) {
    const auto *view = static_cast<const SnapshotView *>(*arg);
    for (uint8_t *breakpoint : view->debugger->breakpoints) {
        const uint32_t address = toVirtualAddress(breakpoint, view->module);
        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_varint(stream, address))
            return false;
    }
    return true;
}

bool encode_functions(pb_ostream_t *stream, const pb_field_t *field,
                      void *const *arg) {
    const auto *view = static_cast<const SnapshotView *>(*arg);
    Module *module = view->module;

    for (uint32_t index = module->import_count; index < module->function_count;
         ++index) {
        const Block &source = module->functions[index];
        if (source.start_ptr == nullptr || source.end_ptr == nullptr ||
            source.end_ptr < source.start_ptr) {
            continue;
        }

        debug_Function function = debug_Function_init_zero;
        function.function_index = source.fidx;
        function.has_range = true;
        function.range.start = toVirtualAddress(source.start_ptr, module);
        function.range.end = toVirtualAddress(source.end_ptr, module);
        ByteView instructions{
            source.start_ptr,
            static_cast<size_t>(source.end_ptr - source.start_ptr + 1)};
        function.instructions.funcs.encode = nanopb_encoder::encode_bytes;
        function.instructions.arg = &instructions;

        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_submessage(stream, debug_Function_fields, &function)) {
            return false;
        }
    }
    return true;
}

bool encode_callstack(pb_ostream_t *stream, const pb_field_t *field,
                      void *const *arg) {
    const auto *view = static_cast<const SnapshotView *>(*arg);
    const ExecutionContext *context = view->context;

    for (int index = 0; index <= context->csp; ++index) {
        const Frame &frame = context->callstack[index];
        const Block *block = frame.block;
        const uint32_t type = block == nullptr ? 0 : block->block_type;
        debug_CallstackEntry entry = debug_CallstackEntry_init_zero;
        entry.type = type;
        entry.function_index = block != nullptr && type == 0 ? block->fidx : 0;
        entry.stack_pointer = static_cast<uint32_t>(frame.sp);
        entry.frame_pointer = static_cast<uint32_t>(frame.fp);
        entry.return_address =
            frame.ra_ptr == nullptr
                ? 0
                : toVirtualAddress(frame.ra_ptr, view->module);

        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_submessage(stream, debug_CallstackEntry_fields,
                                  &entry)) {
            return false;
        }
    }
    return true;
}

bool encode_callback_indexes(pb_ostream_t *stream, const pb_field_t *field,
                             void *const *arg) {
    const auto *callbacks = static_cast<const std::vector<Callback> *>(*arg);
    for (const Callback &callback : *callbacks) {
        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_varint(stream, callback.table_index)) {
            return false;
        }
    }
    return true;
}

bool encode_callbacks(pb_ostream_t *stream, const pb_field_t *field,
                      void *const *arg) {
    const auto *callbacks =
        static_cast<const CallbackHandler::CallbackMap *>(*arg);

    for (const auto &[topic, entries] : *callbacks) {
        debug_CallbackEntry entry = debug_CallbackEntry_init_zero;
        ByteView topicBytes{reinterpret_cast<const uint8_t *>(topic.data()),
                            topic.size()};

        entry.topic.funcs.encode = nanopb_encoder::encode_bytes;
        entry.topic.arg = &topicBytes;
        entry.table_indexes.funcs.encode = encode_callback_indexes;
        entry.table_indexes.arg = entries;

        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_submessage(stream, debug_CallbackEntry_fields, &entry)) {
            return false;
        }
    }
    return true;
}

bool encode_events(pb_ostream_t *stream, const pb_field_t *field,
                   void *const *arg) {
    const auto *range = static_cast<const EventRangeView *>(*arg);

    for (size_t index = 0; index < range->size; ++index) {
        const Event *source = CallbackHandler::event_at(range->begin + index);
        if (source == nullptr) return false;

        debug_Event event = debug_Event_init_zero;
        ByteView topic{reinterpret_cast<const uint8_t *>(source->topic.data()),
                       source->topic.size()};
        ByteView payload{
            reinterpret_cast<const uint8_t *>(source->payload.data()),
            source->payload.size()};
        event.topic.funcs.encode = nanopb_encoder::encode_bytes;
        event.topic.arg = &topic;
        event.payload.funcs.encode = nanopb_encoder::encode_bytes;
        event.payload.arg = &payload;

        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_submessage(stream, debug_Event_fields, &event)) {
            return false;
        }
    }
    return true;
}

bool encode_io_state(pb_ostream_t *stream, const pb_field_t *field,
                     void *const *arg) {
    const auto *states =
        static_cast<const std::vector<IOStateElement *> *>(*arg);
    for (const IOStateElement *source : *states) {
        if (source == nullptr) continue;
        debug_IOState state = debug_IOState_init_zero;
        ByteView key{reinterpret_cast<const uint8_t *>(source->key.data()),
                     source->key.size()};
        state.key.funcs.encode = nanopb_encoder::encode_bytes;
        state.key.arg = &key;
        state.output = source->output;
        state.value = source->value;
        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_submessage(stream, debug_IOState_fields, &state))
            return false;
    }
    return true;
}

bool encode_overrides(pb_ostream_t *stream, const pb_field_t *field,
                      void *const *arg) {
    const auto *view = static_cast<const SnapshotView *>(*arg);

    for (const auto &[key, result] : *view->overrides) {
        if (key.empty()) continue;

        const uint32_t functionIndex = key.back();
        if (functionIndex >= view->module->import_count ||
            view->module->functions[functionIndex].import_field == nullptr) {
            continue;
        }

        debug_Override override = debug_Override_init_zero;
        const char *name = view->module->functions[functionIndex].import_field;
        ByteView nameView{reinterpret_cast<const uint8_t *>(name),
                          strlen(name)};
        Uint32View arguments{key.data(), key.size() - 1};
        override.primitive_name.funcs.encode = nanopb_encoder::encode_bytes;
        override.primitive_name.arg = &nameView;
        override.argument_words.funcs.encode = nanopb_encoder::encode_fixed32s;
        override.argument_words.arg = &arguments;
        override.result = result;

        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_submessage(stream, debug_Override_fields, &override)) {
            return false;
        }
    }
    return true;
}

}  // namespace
