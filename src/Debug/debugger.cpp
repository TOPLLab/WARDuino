#include "debugger.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <optional>

#include "../Memory/mem.h"
#include "../Utils//util.h"
#include "../Utils/macros.h"
#include "../WARDuino/CallbackHandler.h"

// Debugger

Debugger::Debugger(Channel *duplex) {
    this->channel = duplex;
    this->supervisor_mutex = new warduino::mutex();
    this->supervisor_mutex->lock();
    this->snapshotPolicy = SnapshotPolicy::none;
    this->checkpointInterval = 10;
    this->instructions_executed = 0;
    this->instructions_since_full_snapshot = 0;
    this->fidx_called = {};
    this->min_return_values = 0;
    this->checkpoint_state = nullptr;
    this->checkpoint_state_size = 0;
    this->remaining_instructions = -1;
}

// Public methods

void Debugger::setChannel(Channel *duplex) {
    delete this->channel;
    this->channel = duplex;
}

namespace {

bool decodeFrameLength(const std::vector<uint8_t> &bytes, size_t *headerSize,
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

bool isKnownCommand(const uint8_t type) {
    return type <= static_cast<uint8_t>(debug_Command_COMMAND_REMOVE_OVERRIDE);
}

template <typename T>
bool decodePayload(const std::vector<uint8_t> &payload,
                   const pb_msgdesc_t *fields, T *message) {
    pb_istream_t stream =
        pb_istream_from_buffer(payload.data(), payload.size());
    return pb_decode(&stream, fields, message);
}

}  // namespace

void Debugger::addDebugMessage(const size_t len, const uint8_t *buff) {
    if (len == 0 || buff == nullptr) return;
    parseDebugBuffer(len, buff);
}

void Debugger::pushMessage(DebugMessage msg) {
    warduino::lock_guard const lg(messageQueueMutex);
    debugMessages.emplace_back(std::move(msg));
    freshMessages = !debugMessages.empty();
    messageQueueConditionVariable.notify_one();
}

void Debugger::parseDebugBuffer(const size_t len, const uint8_t *buff) {
    pendingFrameBytes.insert(pendingFrameBytes.end(), buff, buff + len);
    while (!pendingFrameBytes.empty()) {
        if (!isKnownCommand(pendingFrameBytes.front())) {
            pendingFrameBytes.clear();
            sendNotification(
                debug_NotificationType_NOTIFICATION_UNKNOWN_COMMAND);
            continue;
        }

        size_t headerSize = 0;
        size_t payloadSize = 0;
        const bool completeLength =
            decodeFrameLength(pendingFrameBytes, &headerSize, &payloadSize);
        if (!completeLength) {
            if (headerSize == SIZE_MAX || pendingFrameBytes.size() >= 6) {
                pendingFrameBytes.clear();
                sendNotification(debug_NotificationType_NOTIFICATION_MALFORMED);
            }
            return;
        }
        if (payloadSize > maxFramePayload) {
            pendingFrameBytes.clear();
            sendNotification(debug_NotificationType_NOTIFICATION_MALFORMED);
            return;
        }
        if (pendingFrameBytes.size() < headerSize + payloadSize) return;

        DebugMessage message{static_cast<debug_Command>(pendingFrameBytes[0]),
                             {}};
        message.payload.assign(
            pendingFrameBytes.begin() + static_cast<std::ptrdiff_t>(headerSize),
            pendingFrameBytes.begin() +
                static_cast<std::ptrdiff_t>(headerSize + payloadSize));
        pendingFrameBytes.erase(
            pendingFrameBytes.begin(),
            pendingFrameBytes.begin() +
                static_cast<std::ptrdiff_t>(headerSize + payloadSize));
        pushMessage(std::move(message));
    }
}

std::optional<DebugMessage> Debugger::getDebugMessage() {
    warduino::lock_guard const lg(messageQueueMutex);
    if (debugMessages.empty()) {
        freshMessages = false;
        return std::nullopt;
    }
    DebugMessage message = std::move(debugMessages.front());
    debugMessages.pop_front();
    freshMessages = !debugMessages.empty();
    return message;
}

bool Debugger::sendNotification(const debug_NotificationType type,
                                const pb_msgdesc_t *fields,
                                const void *payload) const {
    if (channel == nullptr) return false;
    size_t payloadSize = 0;
    if (fields != nullptr && payload != nullptr &&
        !pb_get_encoded_size(&payloadSize, fields, payload)) {
        return false;
    }
    std::vector<uint8_t> frame;
    frame.reserve(1 + 5 + payloadSize);
    frame.push_back(static_cast<uint8_t>(type));
    size_t length = payloadSize;
    do {
        uint8_t byte = static_cast<uint8_t>(length & 0x7fU);
        length >>= 7U;
        if (length != 0) byte |= 0x80U;
        frame.push_back(byte);
    } while (length != 0);
    if (payloadSize != 0) {
        const size_t offset = frame.size();
        frame.resize(offset + payloadSize);
        pb_ostream_t stream =
            pb_ostream_from_buffer(frame.data() + offset, payloadSize);
        if (!pb_encode(&stream, fields, payload)) return false;
    }
    return channel->writeBytes(frame.data(), frame.size()) ==
           static_cast<ssize_t>(frame.size());
}

void Debugger::sendOperationResult(const debug_Command command,
                                   const bool success) const {
    debug_OperationResult result = debug_OperationResult_init_zero;
    result.command = command;
    result.success = success;
    sendNotification(debug_NotificationType_NOTIFICATION_OPERATION_RESULT,
                     debug_OperationResult_fields, &result);
}

void Debugger::addBreakpoint(uint8_t *loc) { this->breakpoints.insert(loc); }

void Debugger::deleteBreakpoint(uint8_t *loc) { this->breakpoints.erase(loc); }

// ReSharper disable once CppParameterMayBeConstPtrOrRef // incorrect warning
bool Debugger::isBreakpoint(uint8_t *loc) {
    return this->breakpoints.find(loc) != this->breakpoints.end() ||
           this->mark == loc;
}

void Debugger::notifyBreakpoint(Module *m, uint8_t *pc_ptr) {
    if (snapshotPolicy == SnapshotPolicy::checkpointing) checkpoint(m);
    mark = nullptr;
    debug_HitBreakpoint hit = debug_HitBreakpoint_init_zero;
    hit.has_location = true;
    hit.location.module_index = 0;
    hit.location.program_counter = toVirtualAddress(pc_ptr, m);
    sendNotification(debug_NotificationType_NOTIFICATION_HIT_BREAKPOINT,
                     debug_HitBreakpoint_fields, &hit);
}

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

bool collectBytes(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<uint8_t> *>(*arg);
    out->resize(stream->bytes_left);
    return out->empty() || pb_read(stream, out->data(), out->size());
}

[[maybe_unused]] bool collectWords(pb_istream_t *stream,
                                   const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<uint32_t> *>(*arg);
    while (stream->bytes_left != 0) {
        uint32_t value = 0;
        if (!pb_decode_fixed32(stream, &value)) return false;
        out->push_back(value);
    }
    return true;
}

void setDecodeCallback(pb_callback_t *callback, std::vector<uint8_t> *out) {
    callback->funcs.decode = collectBytes;
    callback->arg = out;
}

bool collectVarints(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
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
bool collectCallbackEntries(pb_istream_t *stream, const pb_field_iter_t *,
                            void **arg) {
    auto *entries = static_cast<std::vector<DecodedCallbackEntry> *>(*arg);
    debug_CallbackEntry entry = debug_CallbackEntry_init_zero;
    std::vector<uint8_t> topic;
    std::vector<uint32_t> indexes;
    setDecodeCallback(&entry.topic, &topic);
    entry.table_indexes.funcs.decode = collectVarints;
    entry.table_indexes.arg = &indexes;
    if (!pb_decode(stream, debug_CallbackEntry_fields, &entry)) return false;
    entries->push_back(
        {std::string(topic.begin(), topic.end()), std::move(indexes)});
    return true;
}

std::optional<uint32_t> findImportedFunction(Module *m,
                                             const std::string &name) {
    for (uint32_t index = 0; index < m->import_count; ++index) {
        if (m->functions[index].import_field != nullptr &&
            name == m->functions[index].import_field)
            return index;
    }
    return std::nullopt;
}

bool collectValues(pb_istream_t *stream, const pb_field_iter_t *, void **arg) {
    auto *out = static_cast<std::vector<debug_Value> *>(*arg);
    debug_Value value = debug_Value_init_zero;
    if (!pb_decode(stream, debug_Value_fields, &value)) return false;
    out->push_back(value);
    return true;
}

bool valueFromProto(const debug_Value &from, StackValue *to) {
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

[[maybe_unused]] void valueToProto(const StackValue &from, const uint32_t index,
                                   debug_Value *to) {
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

bool encodeValues(pb_ostream_t *stream, const pb_field_t *field,
                  void *const *arg) {
    const auto *values = static_cast<const std::vector<StackValue> *>(*arg);
    for (size_t index = 0; index < values->size(); ++index) {
        debug_Value value = debug_Value_init_zero;
        valueToProto((*values)[index], static_cast<uint32_t>(index), &value);
        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_submessage(stream, debug_Value_fields, &value)) {
            return false;
        }
    }
    return true;
}

bool encodeBytes(pb_ostream_t *stream, const pb_field_t *field,
                 void *const *arg) {
    const auto *bytes = static_cast<const std::vector<uint8_t> *>(*arg);
    return pb_encode_tag_for_field(stream, field) &&
           pb_encode_string(stream, bytes->data(), bytes->size());
}

}  // namespace

bool Debugger::checkDebugMessages(Module *m, RunningState *program_state) {
    std::optional<DebugMessage> message = getDebugMessage();
    if (!message) return false;

    const auto malformed = [this]() {
        sendNotification(debug_NotificationType_NOTIFICATION_MALFORMED);
    };
    const auto requireEmpty = [&message, &malformed]() {
        if (!message->payload.empty()) {
            malformed();
            return false;
        }
        return true;
    };

    switch (message->type) {
        case debug_Command_COMMAND_RUN:
            if (!requireEmpty()) break;
            handleInterruptRUN(m, program_state);
            sendNotification(debug_NotificationType_NOTIFICATION_CONTINUED);
            break;
        case debug_Command_COMMAND_HALT:
            if (!requireEmpty()) break;
            sendNotification(debug_NotificationType_NOTIFICATION_HALTED);
            if (channel != nullptr) channel->close();
            break;
        case debug_Command_COMMAND_PAUSE:
            if (!requireEmpty()) break;
            pauseRuntime(m);
            if (snapshotPolicy == SnapshotPolicy::checkpointing)
                checkpoint(m, true);
            sendNotification(debug_NotificationType_NOTIFICATION_PAUSED);
            break;
        case debug_Command_COMMAND_STEP:
            if (!requireEmpty()) break;
            handleSTEP(m, program_state);
            break;
        case debug_Command_COMMAND_STEP_OVER:
            if (!requireEmpty()) break;
            handleSTEPOver(m, program_state);
            break;
        case debug_Command_COMMAND_ADD_BREAKPOINT:
        case debug_Command_COMMAND_REMOVE_BREAKPOINT: {
            debug_Breakpoint breakpoint = debug_Breakpoint_init_zero;
            if (!decodePayload(message->payload, debug_Breakpoint_fields,
                               &breakpoint) ||
                !breakpoint.has_location ||
                breakpoint.location.module_index != 0 ||
                !isToPhysicalAddrPossible(breakpoint.location.program_counter,
                                          m)) {
                malformed();
                break;
            }
            uint8_t *address =
                toPhysicalAddress(breakpoint.location.program_counter, m);
            if (message->type == debug_Command_COMMAND_ADD_BREAKPOINT)
                addBreakpoint(address);
            else
                deleteBreakpoint(address);
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_CONTINUE_FOR: {
            debug_ContinueFor request = debug_ContinueFor_init_zero;
            if (!decodePayload(message->payload, debug_ContinueFor_fields,
                               &request) ||
                request.count == 0) {
                malformed();
                break;
            }
            remaining_instructions = static_cast<int32_t>(request.count);
            *program_state = WARDUINOrun;
            sendNotification(debug_NotificationType_NOTIFICATION_CONTINUED);
            break;
        }
        case debug_Command_COMMAND_DUMP:
            if (!requireEmpty()) break;
            pauseRuntime(m);
            snapshot(m);
            break;
        case debug_Command_COMMAND_DUMP_LOCALS:
            if (!requireEmpty()) break;
            pauseRuntime(m);
            dumpLocals(m);
            break;
        case debug_Command_COMMAND_SNAPSHOT:
            if (!requireEmpty()) break;
            pauseRuntime(m);
            snapshot(m);
            break;
        case debug_Command_COMMAND_DUMP_EVENTS: {
            debug_Range range = debug_Range_init_zero;
            if (!decodePayload(message->payload, debug_Range_fields, &range) ||
                range.end < range.start) {
                malformed();
                break;
            }
            dumpEvents(range.start, range.end - range.start);
            break;
        }
        case debug_Command_COMMAND_DUMP_CALLBACKS:
            if (!requireEmpty()) break;
            dumpCallbackmapping();
            break;
        case debug_Command_COMMAND_UPDATE_LOCAL:
        case debug_Command_COMMAND_UPDATE_GLOBAL:
        case debug_Command_COMMAND_UPDATE_STACK: {
            debug_ValueUpdate update = debug_ValueUpdate_init_zero;
            if (!decodePayload(message->payload, debug_ValueUpdate_fields,
                               &update) ||
                !update.has_value) {
                malformed();
                break;
            }
            StackValue *value = nullptr;
            if (message->type == debug_Command_COMMAND_UPDATE_LOCAL) {
                ExecutionContext *ectx = m->warduino->execution_context;
                if (ectx->fp + static_cast<int>(update.index) > ectx->sp) {
                    malformed();
                    break;
                }
                value = &ectx->stack[ectx->fp + update.index];
            } else if (message->type == debug_Command_COMMAND_UPDATE_GLOBAL) {
                if (update.index >= m->global_count) {
                    malformed();
                    break;
                }
                value = m->globals[update.index]->value;
            } else {
                ExecutionContext *ectx = m->warduino->execution_context;
                if (update.index > static_cast<uint32_t>(ectx->sp)) {
                    malformed();
                    break;
                }
                value = &ectx->stack[update.index];
            }
            const bool success = valueFromProto(update.value, value);
            if (!success)
                malformed();
            else
                sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_MODULE: {
            debug_ModuleUpdate update = debug_ModuleUpdate_init_zero;
            std::vector<uint8_t> wasm;
            setDecodeCallback(&update.wasm, &wasm);
            if (!decodePayload(message->payload, debug_ModuleUpdate_fields,
                               &update) ||
                wasm.empty()) {
                malformed();
                break;
            }
            auto *copy = static_cast<uint8_t *>(malloc(wasm.size()));
            if (copy == nullptr) {
                sendOperationResult(message->type, false);
                break;
            }
            memcpy(copy, wasm.data(), wasm.size());
            m->warduino->update_module(m, copy, wasm.size());
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_FUNCTION: {
            debug_Function update = debug_Function_init_zero;
            std::vector<uint8_t> instructions;
            setDecodeCallback(&update.instructions, &instructions);
            if (!decodePayload(message->payload, debug_Function_fields,
                               &update) ||
                update.function_index >= m->function_count ||
                instructions.empty() || instructions.back() != 0x0b) {
                malformed();
                break;
            }
            functionBodies[update.function_index] = std::move(instructions);
            Block &function = m->functions[update.function_index];
            function.start_ptr = functionBodies[update.function_index].data();
            function.end_ptr = function.start_ptr +
                               functionBodies[update.function_index].size() - 1;
            function.br_ptr = function.end_ptr;
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_CALLBACKS: {
            debug_CallbackMapping mapping = debug_CallbackMapping_init_zero;
            std::vector<DecodedCallbackEntry> entries;
            mapping.entries.funcs.decode = collectCallbackEntries;
            mapping.entries.arg = &entries;
            if (!decodePayload(message->payload, debug_CallbackMapping_fields,
                               &mapping)) {
                malformed();
                break;
            }
            CallbackHandler::clear_callbacks();
            for (const auto &entry : entries) {
                for (uint32_t index : entry.indexes)
                    CallbackHandler::add_callback(
                        Callback(m, entry.topic, index));
            }
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_SET_SNAPSHOT_POLICY: {
            debug_SnapshotPolicyConfig config =
                debug_SnapshotPolicyConfig_init_zero;
            std::vector<uint8_t> selectedState;
            setDecodeCallback(&config.selected_state, &selectedState);
            if (!decodePayload(message->payload,
                               debug_SnapshotPolicyConfig_fields, &config) ||
                config.policy >
                    debug_SnapshotPolicy_SNAPSHOT_POLICY_CHECKPOINTING) {
                malformed();
                break;
            }
            snapshotPolicy = static_cast<SnapshotPolicy>(config.policy);
            checkpointInterval = config.interval == 0 ? 1 : config.interval;
            min_return_values = config.minimum_return_count;
            free(checkpoint_state);
            checkpoint_state = nullptr;
            checkpoint_state_size = static_cast<uint32_t>(selectedState.size());
            if (!selectedState.empty()) {
                checkpoint_state =
                    static_cast<uint8_t *>(malloc(selectedState.size()));
                if (checkpoint_state == nullptr) {
                    sendOperationResult(message->type, false);
                    break;
                }
                memcpy(checkpoint_state, selectedState.data(),
                       selectedState.size());
            }
            if (snapshotPolicy == SnapshotPolicy::checkpointing)
                checkpoint(m, true);
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_SET_OVERRIDE:
        case debug_Command_COMMAND_REMOVE_OVERRIDE: {
            debug_Override request = debug_Override_init_zero;
            std::vector<uint8_t> nameBytes;
            std::vector<uint32_t> words;
            setDecodeCallback(&request.primitive_name, &nameBytes);
            request.argument_words.funcs.decode = collectWords;
            request.argument_words.arg = &words;
            if (!decodePayload(message->payload, debug_Override_fields,
                               &request)) {
                malformed();
                break;
            }
            const auto fidx = findImportedFunction(
                m, std::string(nameBytes.begin(), nameBytes.end()));
            if (!fidx ||
                words.size() != m->functions[*fidx].type->param_count) {
                sendOperationResult(message->type, false);
                break;
            }
            words.push_back(*fidx);
            if (message->type == debug_Command_COMMAND_SET_OVERRIDE)
                overrides[words] = request.result;
            else if (overrides.erase(words) == 0) {
                sendOperationResult(message->type, false);
                break;
            }
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_INSPECT: {
            debug_Inspect inspectRequest = debug_Inspect_init_zero;
            std::vector<uint8_t> ignored;
            setDecodeCallback(&inspectRequest.state, &ignored);
            if (!decodePayload(message->payload, debug_Inspect_fields,
                               &inspectRequest)) {
                malformed();
                break;
            }
            snapshot(m);
            break;
        }
        case debug_Command_COMMAND_LOAD_SNAPSHOT: {
            debug_Snapshot state = debug_Snapshot_init_zero;
            if (!decodePayload(message->payload, debug_Snapshot_fields,
                               &state) ||
                !isToPhysicalAddrPossible(state.program_counter, m)) {
                malformed();
                break;
            }
            pauseRuntime(m);
            m->warduino->execution_context->pc_ptr =
                toPhysicalAddress(state.program_counter, m);
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_ADD_PROXY:
        case debug_Command_COMMAND_REMOVE_PROXY: {
            debug_FunctionRef reference = debug_FunctionRef_init_zero;
            if (!decodePayload(message->payload, debug_FunctionRef_fields,
                               &reference) ||
                supervisor == nullptr ||
                reference.function_index >= m->function_count) {
                sendOperationResult(message->type, false);
                break;
            }
            if (message->type == debug_Command_COMMAND_ADD_PROXY)
                supervisor->registerProxiedCall(reference.function_index);
            else
                supervisor->unregisterProxiedCall(reference.function_index);
            sendOperationResult(message->type, true);
            break;
        }
        case debug_Command_COMMAND_PROXY_CALL:
        case debug_Command_COMMAND_INVOKE: {
            debug_RemoteFunctionCall call = debug_RemoteFunctionCall_init_zero;
            std::vector<debug_Value> values;
            call.arguments.funcs.decode = collectValues;
            call.arguments.arg = &values;
            if (!decodePayload(message->payload,
                               debug_RemoteFunctionCall_fields, &call) ||
                call.function_index >= m->function_count ||
                values.size() !=
                    m->functions[call.function_index].type->param_count) {
                malformed();
                break;
            }
            auto *arguments = new StackValue[values.size()];
            bool valid = true;
            for (size_t index = 0; index < values.size(); ++index)
                valid &= valueFromProto(values[index], &arguments[index]);
            if (!valid) {
                delete[] arguments;
                malformed();
                break;
            }
            if (message->type == debug_Command_COMMAND_PROXY_CALL) {
                if (proxy == nullptr) {
                    delete[] arguments;
                    sendOperationResult(message->type, false);
                    break;
                }
                proxy->pushRFC(
                    m,
                    new RFC(call.function_index,
                            m->functions[call.function_index].type, arguments));
                break;
            }
            const RunningState current = m->warduino->program_state;
            m->warduino->program_state = WARDUINOrun;
            exception[0] = "\0"[0];
            const auto results = m->warduino->invoke(
                m, call.function_index, static_cast<uint32_t>(values.size()),
                arguments);
            m->warduino->program_state = current;
            delete[] arguments;

            debug_RemoteFunctionResult result =
                debug_RemoteFunctionResult_init_zero;
            result.success = exception[0] == "\0"[0];
            std::vector<uint8_t> error;
            if (result.success) {
                result.results.funcs.encode = encodeValues;
                result.results.arg =
                    const_cast<std::vector<StackValue> *>(&results);
            } else {
                error.assign(exception, exception + std::strlen(exception));
                result.error.funcs.encode = encodeBytes;
                result.error.arg = &error;
            }
            sendNotification(
                debug_NotificationType_NOTIFICATION_REMOTE_FUNCTION_RESULT,
                debug_RemoteFunctionResult_fields, &result);
            break;
        }
        case debug_Command_COMMAND_PROXIFY:
            if (!requireEmpty()) break;
            proxify();
            sendOperationResult(message->type, true);
            break;
        case debug_Command_COMMAND_POP_EVENT:
            if (!requireEmpty()) break;
            sendOperationResult(message->type,
                                CallbackHandler::resolve_event(true));
            break;
        case debug_Command_COMMAND_PUSH_EVENT: {
            debug_Event event = debug_Event_init_zero;
            std::vector<uint8_t> topic;
            std::vector<uint8_t> payload;
            setDecodeCallback(&event.topic, &topic);
            setDecodeCallback(&event.payload, &payload);
            if (!decodePayload(message->payload, debug_Event_fields, &event) ||
                topic.empty()) {
                malformed();
                break;
            }
            CallbackHandler::push_event(
                std::string(topic.begin(), topic.end()),
                reinterpret_cast<const char *>(payload.data()), payload.size());
            notifyPushedEvent();
            break;
        }
        case debug_Command_COMMAND_RESET:
            if (!requireEmpty()) break;
            sendOperationResult(message->type, reset(m));
            break;
        default:
            malformed();
            break;
    }
    return true;
}

// Private methods
void Debugger::printValue(const StackValue *, const uint32_t,
                          const bool) const {}

uint8_t *Debugger::findOpcode(Module *m, const Block *block) {
    const auto find =
        std::find_if(std::begin(m->block_lookup), std::end(m->block_lookup),
                     [&](const std::pair<uint8_t *, Block *> &pair) {
                         return pair.second == block;
                     });
    uint8_t *opcode = nullptr;
    if (find != std::end(m->block_lookup)) {
        opcode = find->first;
    } else {
        // FIXME FATAL?
        debug("find_opcode: not found\n");
        exit(33);
    }
    return opcode;
}

void Debugger::handleInvoke(Module *m, uint8_t *interruptData) const {
    const uint32_t fidx = read_LEB_32(&interruptData);

    if (fidx >= m->function_count) {
        debug("no function available for fidx %" PRIu32 "\n", fidx);
        return;
    }

    const Type func = *m->functions[fidx].type;
    StackValue *args = readWasmArgs(func, interruptData);

    WARDuino *instance = WARDuino::instance();
    const RunningState current = instance->program_state;
    instance->program_state = WARDUINOrun;

    WARDuino::instance()->invoke(m, fidx, func.param_count, args);
    instance->program_state = current;
    this->dumpStack(m);
}

void Debugger::handleInterruptRUN(const Module *m,
                                  RunningState *program_state) {
    ExecutionContext *ectx = m->warduino->execution_context;
    if (*program_state == WARDUINOpause && this->isBreakpoint(ectx->pc_ptr)) {
        this->skipBreakpoint = ectx->pc_ptr;
    }
    *program_state = WARDUINOrun;
}

void Debugger::handleSTEP(const Module *m, RunningState *program_state) {
    ExecutionContext *ectx = m->warduino->execution_context;
    *program_state = WARDUINOstep;
    this->skipBreakpoint = ectx->pc_ptr;
}

void Debugger::handleSTEPOver(const Module *m, RunningState *program_state) {
    ExecutionContext *ectx = m->warduino->execution_context;
    this->skipBreakpoint = ectx->pc_ptr;
    uint8_t const opcode = *ectx->pc_ptr;
    if (opcode == 0x10) {  // step over direct call
        uint8_t *ptr_cpy = ectx->pc_ptr + 1;
        read_LEB_32(&ptr_cpy);
        this->mark = ectx->pc_ptr + (ptr_cpy - ectx->pc_ptr);
        *program_state = WARDUINOrun;
        // warning: ack will be BP hit
    } else if (opcode == 0x11) {  // step over indirect call
        uint8_t *ptr_cpy = ectx->pc_ptr + 1;
        read_LEB_32(&ptr_cpy);
        read_LEB_32(&ptr_cpy);
        this->mark = ectx->pc_ptr + (ptr_cpy - ectx->pc_ptr);
        *program_state = WARDUINOrun;
    } else {
        // normal step
        this->handleSTEP(m, program_state);
    }
}

void Debugger::handleInterruptBP(Module *m, uint8_t *interruptData) {
    uint8_t *bpData = interruptData + 1;
    uint32_t virtualAddress = read_B32(&bpData);
    if (isToPhysicalAddrPossible(virtualAddress, m)) {
        uint8_t *bpt = toPhysicalAddress(virtualAddress, m);
        if (*interruptData == 0x06) {
            this->addBreakpoint(bpt);
        } else {
            this->deleteBreakpoint(bpt);
        }
    }
    debug("BP %" PRIu32 "!\n", virtualAddress);
}

void Debugger::dump(Module *m, bool) const { snapshot(m); }

void Debugger::dumpStack(const Module *) const {
    debug_Locals locals = debug_Locals_init_zero;
    sendNotification(debug_NotificationType_NOTIFICATION_LOCALS_DUMP,
                     debug_Locals_fields, &locals);
}

void Debugger::dumpBreakpoints(Module *) const {}

void Debugger::dumpFunctions(Module *) const {}

/*
 * {"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s
 */
void Debugger::dumpCallstack(Module *) const {}

void Debugger::dumpLocals(const Module *) const {
    debug_Locals locals = debug_Locals_init_zero;
    sendNotification(debug_NotificationType_NOTIFICATION_LOCALS_DUMP,
                     debug_Locals_fields, &locals);
}

void Debugger::dumpEvents(long start, long size) const {
    debug_EventsQueue events = debug_EventsQueue_init_zero;
    events.total_count = static_cast<uint32_t>(CallbackHandler::event_count());
    events.has_range = true;
    events.range.start = start < 0 ? 0 : static_cast<uint32_t>(start);
    events.range.end = size < 0
                           ? events.range.start
                           : events.range.start + static_cast<uint32_t>(size);
    sendNotification(debug_NotificationType_NOTIFICATION_EVENTS_DUMP,
                     debug_EventsQueue_fields, &events);
}

void Debugger::dumpCallbackmapping() const {
    debug_CallbackMapping callbacks = debug_CallbackMapping_init_zero;
    sendNotification(debug_NotificationType_NOTIFICATION_CALLBACKS_DUMP,
                     debug_CallbackMapping_fields, &callbacks);
}

void Debugger::dumpHeapInfo(Module *) const {}

/**
 * Read the change in bytes array.
 *
 * The array should be of the form
 * [0x10, index, ... new function body 0x0b]
 * Where index is the index without imports
 */
bool Debugger::handleChangedFunction(const Module *m, uint8_t *bytes) {
    // Check if this was a change request
    if (*bytes != interruptUPDATEFun) return false;

    // SKIP the first byte (0x10), type of change
    uint8_t *pos = bytes + 1;

    uint32_t b = read_LEB_32(&pos);  // read id

    Block *function = &m->functions[m->import_count + b];
    const uint32_t body_size = read_LEB_32(&pos);
    uint8_t *payload_start = pos;
    const uint32_t local_count = read_LEB_32(&pos);
    uint8_t *save_pos = pos;
    uint32_t tidx, lidx, lecount;

    // Local variable handling

    // Get number of locals for alloc
    function->local_count = 0;
    for (uint32_t l = 0; l < local_count; l++) {
        lecount = read_LEB_32(&pos);
        function->local_count += lecount;
        tidx = read_LEB(&pos, 7);
        (void)tidx;  // TODO: use tidx?
    }

    if (function->local_count > 0) {
        function->local_value_type = static_cast<uint8_t *>(
            acalloc(function->local_count, sizeof(uint8_t),
                    "function->local_value_type"));
    }

    // Restore position and read the locals
    pos = save_pos;
    lidx = 0;
    for (uint32_t l = 0; l < local_count; l++) {
        lecount = read_LEB_32(&pos);
        uint8_t vt = read_LEB(&pos, 7);
        for (uint32_t i = 0; i < lecount; i++) {
            function->local_value_type[lidx++] = vt;
        }
    }

    function->start_ptr = pos;
    function->end_ptr = payload_start + body_size - 1;
    function->br_ptr = function->end_ptr;
    ASSERT(*function->end_ptr == 0x0b, "Code section did not end with 0x0b\n");
    pos = function->end_ptr + 1;
    return true;
}

/**
 * Read change to local
 * @param m
 * @param bytes
 * @return
 */
bool Debugger::handleChangedLocal(const Module *m, uint8_t *bytes) const {
    if (*bytes != interruptUPDATELocal) return false;
    uint8_t *pos = bytes + 1;
    debug("Local updates: %x\n", *pos);
    uint32_t localId = read_LEB_32(&pos);

    debug("Local %u being changed\n", localId);
    ExecutionContext *ectx = m->warduino->execution_context;
    auto v = &ectx->stack[ectx->fp + localId];
    switch (v->value_type) {
        case I32:
            v->value.uint32 = read_LEB_signed(&pos, 32);
            break;
        case I64:
            v->value.int64 = static_cast<int64_t>(read_LEB_signed(&pos, 64));
            break;
        case F32:
            memcpy(&v->value.uint32, pos, 4);
            break;
        case F64:
            memcpy(&v->value.uint64, pos, 8);
            break;
        default:  // nothing to do :(
            break;
    }
    debug("Local %u changed to %u\n", localId, v->value.uint32);
    return true;
}

void Debugger::notifyPushedEvent() const {
    this->sendNotification(debug_NotificationType_NOTIFICATION_NEW_EVENT);
}

bool Debugger::handlePushedEvent(char *) const { return false; }

void Debugger::snapshot(Module *m) const {
    ExecutionContext *ectx = m->warduino->execution_context;
    debug_Snapshot state = debug_Snapshot_init_zero;
    state.program_counter = toVirtualAddress(ectx->pc_ptr, m);
    state.heap_used = m->warduino->get_heap_used();
    switch (m->warduino->program_state) {
        case WARDUINOrun:
            state.state = debug_State_STATE_WARDUINO_RUN;
            break;
        case WARDUINOpause:
            state.state = debug_State_STATE_WARDUINO_PAUSE;
            break;
        case WARDUINOstep:
            state.state = debug_State_STATE_WARDUINO_STEP;
            break;
        case PROXYrun:
            state.state = debug_State_STATE_PROXY_RUN;
            break;
        case PROXYhalt:
            state.state = debug_State_STATE_PROXY_HALT;
            break;
        default:
            state.state = debug_State_STATE_WARDUINO_PAUSE;
            break;
    }
    sendNotification(debug_NotificationType_NOTIFICATION_SNAPSHOT,
                     debug_Snapshot_fields, &state);
}

void Debugger::inspect(Module *m, const uint16_t, const uint8_t *) const {
    snapshot(m);
}

void Debugger::setSnapshotPolicy(Module *m, uint8_t *interruptData) {
    uint8_t **data_ptr = &interruptData;
    if (*interruptData <= 2) {
        snapshotPolicy = SnapshotPolicy{*interruptData};
        min_return_values = 0;
        if (checkpoint_state) {
            free(checkpoint_state);
        }
        checkpoint_state = nullptr;
        checkpoint_state_size = 0;
        *data_ptr += 1;
    } else {
        snapshotPolicy = SnapshotPolicy::checkpointing;
        *data_ptr += 1;
        min_return_values = read_LEB_32(data_ptr);
        if (checkpoint_state) {
            free(checkpoint_state);
        }
        checkpoint_state_size = read_LEB_32(data_ptr);
        checkpoint_state = new uint8_t[checkpoint_state_size];
        for (uint32_t i = 0; i < checkpoint_state_size; i++) {
            checkpoint_state[i] = **data_ptr;
            *data_ptr += 1;
        }
    }

    // Make a checkpoint when you first enable checkpointing
    if (snapshotPolicy == SnapshotPolicy::checkpointing) {
        checkpointInterval = read_B32(data_ptr);
        instructions_executed = 0;
        instructions_since_full_snapshot = 0;
        checkpoint(m, true);
    }
    printf("ack%x\n", interruptSetSnapshotPolicy);
}

std::optional<uint32_t> getPrimitiveBeingCalled(Module *m, uint8_t *pc_ptr) {
    if (!pc_ptr) {
        return std::nullopt;
    }

    // TODO: Support call_indirect
    uint8_t opcode = *pc_ptr;
    if (opcode == 0x10) {  // call opcode
        uint8_t *pc_copy = pc_ptr + 1;
        uint32_t fidx = read_LEB_32(&pc_copy);
        if (fidx < m->import_count) {
            return fidx;
        }
    }
    return std::nullopt;
}

void Debugger::handleSnapshotPolicy(Module *m) {
    if (snapshotPolicy == SnapshotPolicy::atEveryInstruction) {
        debug("SNAPSHOT ");
        snapshot(m);
        debug("\n");
    } else if (snapshotPolicy == SnapshotPolicy::checkpointing) {
        if (instructions_executed >= checkpointInterval || fidx_called) {
            if (min_return_values == 0) {
                checkpoint(m);
            } else {
                if (fidx_called) {
                    const Type *type = m->functions[*fidx_called].type;
                    if (type->result_count >= min_return_values) {
                        checkpoint(m);
                    }
                }
            }
        }

        // When using tracing, optionally (if the interval is 0xffffffff no full
        // snapshots will be taken) take full checkpoints every
        // checkpointInterval instructions.
        if (checkpoint_state != nullptr) {
            if (checkpointInterval != UINT32_MAX &&
                instructions_since_full_snapshot >= checkpointInterval) {
                checkpoint(m, true, true);
                instructions_since_full_snapshot = 0;
            }
            instructions_since_full_snapshot++;
        }

        instructions_executed++;

        ExecutionContext *ectx = m->warduino->execution_context;
        // Store arguments of last primitive call.
        if ((fidx_called = getPrimitiveBeingCalled(m, ectx->pc_ptr))) {
            const Type *type = m->functions[*fidx_called].type;
            for (uint32_t i = 0; i < type->param_count; i++) {
                prim_args[type->param_count - i - 1] =
                    ectx->stack[ectx->sp - i].value.uint32;
            }
        }
    } else if (snapshotPolicy != SnapshotPolicy::none) {
        debug("WARNING: Invalid snapshot policy.");
    }
}

void Debugger::checkpoint(Module *, const bool force, const bool) {
    if (instructions_executed == 0 && !force) {
        return;
    }

    debug_Checkpoint notification = debug_Checkpoint_init_zero;
    notification.instruction_count = instructions_executed;
    if (fidx_called) {
        notification.has_primitive_call = true;
        notification.primitive_function_index = *fidx_called;
    }
    sendNotification(debug_NotificationType_NOTIFICATION_CHECKPOINT,
                     debug_Checkpoint_fields, &notification);
    instructions_executed = 0;
}

void Debugger::freeState(Module *m, uint8_t *interruptData) {
    debug("freeing the program state\n");
    uint8_t *first_msg = nullptr;
    uint8_t *endfm = nullptr;
    first_msg = interruptData + 1;  // skip interruptLoadSnapshot
    endfm = first_msg + read_B32(&first_msg);

    // nullify state
    this->breakpoints.clear();
    ExecutionContext *ectx = m->warduino->execution_context;
    ectx->csp = -1;
    ectx->sp = -1;
    memset(ectx->br_table, 0, BR_TABLE_SIZE);

    // Reset checkpointing counters, new checkpoints will have instructions
    // executed since this snapshot.
    instructions_since_full_snapshot = 0;
    instructions_executed = 0;

    while (first_msg < endfm) {
        switch (*first_msg++) {
            case globalsState: {
                debug("receiving globals info\n");
                uint32_t amount = read_B32(&first_msg);
                debug("total globals %d\n", amount);
                // TODO if global_count != amount Otherwise set all to zero
                if (m->global_count != amount) {
                    debug("globals freeing state and then allocating\n");
                    if (m->global_count > 0) free(m->globals);
                    if (amount > 0)
                        m->globals = static_cast<Global **>(
                            acalloc(amount, sizeof(Global *), "globals"));
                } else {
                    debug("globals setting existing state to zero\n");
                    for (uint32_t i = 0; i < m->global_count; i++) {
                        debug("decreasing global_count\n");
                        Global *glob = m->globals[i];
                        glob->value->value_type = 0;
                        glob->value->value.uint32 = 0;
                    }
                }
                m->global_count = 0;
                break;
            }
            case tableState: {
                debug("receiving table info\n");
                m->table.initial = read_B32(&first_msg);
                m->table.maximum = read_B32(&first_msg);
                uint32_t size = read_B32(&first_msg);
                debug("init %d max %d size %d\n", m->table.initial,
                      m->table.maximum, size);
                if (m->table.size != size) {
                    debug("old table size %d\n", m->table.size);
                    if (m->table.size != 0) free(m->table.entries);
                    m->table.entries = static_cast<uint32_t *>(acalloc(
                        size, sizeof(uint32_t), "Module->table.entries"));
                }
                m->table.size = 0;  // allows to accumulatively add entries
                break;
            }
            case memoryState: {
                debug("receiving memory info\n");
                // FIXME: init & max not needed
                m->memory.maximum = read_B32(&first_msg);
                m->memory.initial = read_B32(&first_msg);
                uint32_t pages = read_B32(&first_msg);
                debug("max %d init %d current page %d\n", m->memory.maximum,
                      m->memory.initial, pages);
                // if(pages !=m->memory.pages){
                // if(m->memory.pages !=0)
                if (m->memory.bytes != nullptr) {
                    free(m->memory.bytes);
                }
                m->memory.bytes = static_cast<uint8_t *>(
                    acalloc(pages * PAGE_SIZE, 1, "Module->memory.bytes"));
                m->memory.pages = pages;
                // }
                // else{
                //   //TODO fill memory.bytes with zeros
                // memset(m->memory.bytes, 0, m->memory.pages * PAGE_SIZE) ;
                // }
                break;
            }
            default:
                FATAL("freeState: receiving unknown command\n");
        }
    }
    debug("done with first msg\n");
}

bool Debugger::saveState(Module *m, uint8_t *interruptData) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint8_t *program_state = nullptr;
    uint8_t *end_state = nullptr;
    program_state = interruptData + 1;  // skip interruptLoadSnapshot
    uint32_t len = read_B32(&program_state);
    end_state = program_state + len;

    debug("saving program_state\n");
    while (program_state < end_state) {
        switch (*program_state++) {
            case pcState: {  // PC
                uint32_t pc = read_B32(&program_state);
                if (!isToPhysicalAddrPossible(pc, m)) {
                    FATAL("cannot set pc on invalid address\n");
                }
                ectx->pc_ptr = toPhysicalAddress(pc, m);
                debug("Updated pc %" PRIu32 "\n", pc);
                break;
            }
            case breakpointsState: {  // breakpoints
                uint8_t quantity_bps = *program_state++;
                debug("receiving breakpoints %" PRIu8 "\n", quantity_bps);
                for (size_t i = 0; i < quantity_bps; i++) {
                    auto virtualBP = read_B32(&program_state);
                    if (isToPhysicalAddrPossible(virtualBP, m)) {
                        this->addBreakpoint(toPhysicalAddress(virtualBP, m));
                    }
                }
                break;
            }
            case callstackState: {
                debug("receiving callstack\n");
                uint16_t quantity = read_B16(&program_state);
                debug("quantity frames %" PRIu16 "\n", quantity);
                /* printf("quantity frames %" PRIu16 "\n", quantity); */
                for (size_t i = 0; i < quantity; i++) {
                    /* printf("frame IDX: %lu\n", i); */
                    uint8_t block_type = *program_state++;
                    ectx->csp += 1;
                    Frame *f = ectx->callstack + ectx->csp;
                    f->sp = read_B32_signed(&program_state);
                    f->fp = read_B32_signed(&program_state);
                    auto virtualRA = read_B32_signed(&program_state);
                    f->ra_ptr = virtualRA >= 0 ? toPhysicalAddress(virtualRA, m)
                                               : nullptr;
                    if (block_type == 0) {  // a function
                        debug("function block\n");
                        uint32_t fidx = read_B32(&program_state);
                        /* debug("function block idx=%" PRIu32 "\n", fidx); */
                        f->block = m->functions + fidx;

                        if (f->block->fidx != fidx) {
                            FATAL("incorrect fidx: exp %" PRIu32 " got %" PRIu32
                                  ". Exiting program\n",
                                  fidx, f->block->fidx);
                        }
                        ectx->fp = f->sp + 1;
                    } else if (block_type == 0xff || block_type == 0xfe) {
                        debug("guard block %" PRIu8 "\n", block_type);
                        auto *guard =
                            static_cast<Block *>(malloc(sizeof(struct Block)));
                        guard->block_type = block_type;
                        guard->type = nullptr;
                        guard->local_value_type = nullptr;
                        guard->start_ptr = nullptr;
                        guard->end_ptr = nullptr;
                        guard->else_ptr = nullptr;
                        guard->export_name = nullptr;
                        guard->import_field = nullptr;
                        guard->import_module = nullptr;
                        guard->func_ptr = nullptr;
                        f->block = guard;
                    } else {
                        debug("non function block\n");
                        auto virtualBK = read_B32(&program_state);
                        auto *block_key = toPhysicalAddress(virtualBK, m);
                        /* debug("block_key=%p\n", static_cast<void
                         * *>(block_key)); */
                        f->block = m->block_lookup[block_key];
                        if (f->block == nullptr) {
                            FATAL("block_lookup cannot be nullptr\n");
                        }
                    }
                }
                break;
            }
            case globalsState: {  // TODO merge globalsState stackState into
                                  // one case
                debug("receiving global state\n");
                uint32_t quantity_globals = read_B32(&program_state);
                uint8_t valtypes[] = {I32, I64, F32, F64};

                debug("receiving #%" PRIu32 " globals\n", quantity_globals);
                for (uint32_t q = 0; q < quantity_globals; q++) {
                    uint8_t type_index = *program_state++;
                    if (type_index >= sizeof(valtypes)) {
                        FATAL("received unknown type %" PRIu8 "\n", type_index);
                    }
                    StackValue *sv = m->globals[m->global_count++]->value;
                    size_t qb = type_index == 0 || type_index == 2 ? 4 : 8;
                    debug("receiving type %" PRIu8 " and %d bytes \n",
                          type_index,
                          type_index == 0 || type_index == 2 ? 4 : 8);

                    sv->value_type = valtypes[type_index];
                    memcpy(&sv->value, program_state, qb);
                    program_state += qb;
                }
                break;
            }
            case tableState: {
                uint32_t quantity = read_B32(&program_state);
                for (size_t i = 0; i < quantity; i++) {
                    uint32_t ne = read_B32(&program_state);
                    m->table.entries[m->table.size++] = ne;
                }
                break;
            }
            case memoryState: {
                debug("receiving memory\n");
                uint32_t start = read_B32(&program_state);
                uint32_t limit = read_B32(&program_state);
                if (start > limit) {
                    FATAL("incorrect memory offsets\n");
                }
                uint32_t total_bytes = limit - start + 1;
                uint8_t *mem_end =
                    m->memory.bytes +
                    m->memory.pages * static_cast<uint32_t>(PAGE_SIZE);
                debug("will copy #%" PRIu32 " bytes from %" PRIu32
                      " to %" PRIu32 " (incl.)\n",
                      total_bytes, start, limit);
                if ((m->memory.bytes + start) + total_bytes > mem_end) {
                    FATAL("memory overflow %p > %p\n",
                          static_cast<void *>(m->bytes + start + total_bytes),
                          static_cast<void *>(mem_end));
                }

                uint32_t byte_count = read_B32(&program_state);
                uint8_t *end_pos = program_state + byte_count;
                uint32_t current_pos = start;
                while (program_state < end_pos) {
                    uint32_t count = read_LEB_32(&program_state);
                    uint8_t byte = *program_state++;
                    memset(m->memory.bytes + current_pos, byte, count);
                    current_pos += count;
                }
                if (current_pos != limit + 1) {
                    FATAL("RLE did not restore the expected amount of bytes\n");
                }

                for (auto i = start; i < (start + total_bytes); i++) {
                    debug("GOT byte idx %" PRIu32 " =%" PRIu8 "\n", i,
                          m->memory.bytes[i]);
                }
                break;
            }
            case branchingTableState: {
                debug("receiving br_table\n");
                uint16_t begin_index = read_B16(&program_state);
                uint16_t end_index = read_B16(&program_state);
                debug("br_table offsets begin=%" PRIu16 " , end=%" PRIu16 "\n",
                      begin_index, end_index);
                if (begin_index > end_index) {
                    FATAL("incorrect br_table offsets\n");
                }
                if (end_index >= BR_TABLE_SIZE) {
                    FATAL("br_table overflow\n");
                }
                for (auto idx = begin_index; idx <= end_index; idx++) {
                    // FIXME speedup with memcpy?
                    uint32_t el = read_B32(&program_state);
                    ectx->br_table[idx] = el;
                }
                break;
            }
            case stackState: {
                // FIXME the float does add numbers at the end. The extra
                // numbers are present in the send information when dump occurs
                debug("receiving stack\n");
                uint16_t quantity_sv = read_B16(&program_state);
                uint8_t valtypes[] = {I32, I64, F32, F64};
                for (size_t i = 0; i < quantity_sv; i++) {
                    uint8_t type_index = *program_state++;
                    if (type_index >= sizeof(valtypes)) {
                        FATAL("received unknown type %" PRIu8 "\n", type_index);
                    }
                    ectx->sp += 1;
                    StackValue *sv = &ectx->stack[ectx->sp];
                    sv->value.uint64 = 0;  // init whole union to 0
                    size_t qb = type_index == 0 || type_index == 2 ? 4 : 8;
                    sv->value_type = valtypes[type_index];
                    memcpy(&sv->value, program_state, qb);
                    program_state += qb;
                }
                break;
            }
            case callbacksState: {
                uint32_t numberMappings = read_B32(&program_state);
                for (auto idx = 0u; idx < numberMappings; ++idx) {
                    uint32_t callbackKeySize = read_B32(&program_state);
                    auto *callbackKey =
                        static_cast<char *>(malloc(callbackKeySize + 1));
                    memcpy(callbackKey, program_state, callbackKeySize);
                    callbackKey[callbackKeySize] = '\0';
                    program_state += callbackKeySize;
                    std::string key{callbackKey};
                    free(callbackKey);
                    uint32_t numberTableIndexes = read_B32(&program_state);
                    for (auto j = 0u; j < numberTableIndexes; ++j) {
                        uint32_t tidx = read_B32(&program_state);
                        CallbackHandler::add_callback(Callback(m, key, tidx));
                    }
                }
                break;
            }
            case eventsState: {
                uint32_t numberEvents = read_B32(&program_state);
                for (auto idx = 0u; idx < numberEvents; ++idx) {
                    // read topic
                    uint32_t topicSize = read_B32(&program_state);
                    auto *topic = static_cast<char *>(malloc(topicSize + 1));
                    memcpy(topic, program_state, topicSize);
                    topic[topicSize] = '\0';
                    program_state += topicSize;

                    // read payload
                    uint32_t payloadSize = read_B32(&program_state);
                    auto *payload =
                        static_cast<char *>(malloc(payloadSize + 1));
                    memcpy(payload, program_state, payloadSize);
                    payload[payloadSize] = '\0';
                    program_state += payloadSize;

                    CallbackHandler::push_event(topic, payload, payloadSize);
                    free(topic);
                }
                break;
            }
            case ioState: {
                debug("receiving ioState\n");
                uint8_t io_state_count = *program_state++;
                std::vector<IOStateElement> external_state;
                external_state.reserve(io_state_count);
                for (int i = 0; i < io_state_count; i++) {
                    IOStateElement state_elem;
                    state_elem.key = "";
                    char c = static_cast<char>(*program_state++);
                    while (c != '\0') {
                        state_elem.key += c;
                        c = static_cast<char>(*program_state++);
                    }
                    state_elem.output = *program_state++;
                    state_elem.value =
                        static_cast<int>(read_B32(&program_state));
                    external_state.emplace_back(state_elem);
                    debug("pin %s(%s) = %d\n", state_elem.key.c_str(),
                          state_elem.output ? "output" : "input",
                          state_elem.value);
                }
                m->warduino->interpreter->restore_external_state(
                    m, external_state);
                break;
            }
            case overridesState: {
                debug("receiving overridesState\n");
                overrides.clear();
                uint8_t overrides_count = *program_state++;
                for (uint32_t i = 0; i < overrides_count; i++) {
                    uint32_t fidx = read_B32(&program_state);
                    uint32_t param_count = m->functions[fidx].type->param_count;
                    std::vector<uint32_t> key(param_count + 1);
                    for (uint32_t j = 0; j < param_count; j++) {
                        key[j] = read_B32(&program_state);
                    }
                    key[param_count] = fidx;
                    uint32_t return_value = read_B32(&program_state);
                    overrides[key] = return_value;
                }
                break;
            }
            default: {
                FATAL("saveState: Received unknown program state\n");
            }
        }
    }
    auto done = *program_state;
    return done == static_cast<uint8_t>(1);
}

uintptr_t Debugger::readPointer(uint8_t **data) {
    const uint8_t len = (*data)[0];
    uintptr_t bp = 0x0;
    for (size_t i = 0; i < len; i++) {
        bp <<= sizeof(uint8_t) * 8;
        bp |= (*data)[i + 1];
    }
    *data += 1 + len;  // skip pointer
    return bp;
}

void Debugger::proxify() {
    WARDuino::instance()->program_state = PROXYhalt;
    this->proxy = new Proxy();  // TODO delete
}

void Debugger::handleProxyCall(Module *m, RunningState *,
                               uint8_t *interruptData) const {
    if (this->proxy == nullptr) {
        dbg_info("No proxy available to send proxy call to.\n");
        // TODO how to handle this error?
        return;
    }
    uint8_t *data = interruptData;
    uint32_t fidx = read_L32(&data);
    dbg_info("Proxycall func %" PRIu32 "\n", fidx);

    Block *func = &m->functions[fidx];
    StackValue *args = Proxy::readRFCArgs(func, data);
    dbg_trace("Enqueuing callee %" PRIu32 "\n", func->fidx);

    auto *rfc = new RFC(fidx, func->type, args);
    this->proxy->pushRFC(m, rfc);
}

RFC *Debugger::topProxyCall() const {
    if (proxy == nullptr) {
        return nullptr;
    }
    return this->proxy->topRFC();
}

void Debugger::sendProxyCallResult(Module *m) const {
    if (proxy == nullptr) return;
    RFC *rfc = proxy->returnResult(m);
    if (rfc == nullptr) return;
    debug_RemoteFunctionResult result = debug_RemoteFunctionResult_init_zero;
    result.success = rfc->success;
    sendNotification(debug_NotificationType_NOTIFICATION_REMOTE_FUNCTION_RESULT,
                     debug_RemoteFunctionResult_fields, &result);
    delete rfc;
}

bool Debugger::isProxy() const { return this->proxy != nullptr; }

bool Debugger::isProxied(const uint32_t fidx) const {
    return this->supervisor != nullptr && this->supervisor->isProxied(fidx);
}

void Debugger::handleMonitorProxies(const Module *m,
                                    uint8_t *interruptData) const {
    const uint32_t amount_funcs = read_B32(&interruptData);
    printf("funcs_total %" PRIu32 "\n", amount_funcs);

    m->warduino->debugger->supervisor->unregisterAllProxiedCalls();
    for (uint32_t i = 0; i < amount_funcs; i++) {
        const uint32_t fidx = read_B32(&interruptData);
        printf("registering fid=%" PRIu32 "\n", fidx);
        m->warduino->debugger->supervisor->registerProxiedCall(fidx);
    }

    debug("done!\n");
}

void Debugger::startProxySupervisor(Channel *socket) {
    this->connected_to_proxy = true;
    this->supervisor = new ProxySupervisor(socket, this->supervisor_mutex);
    printf("Connected to proxy.\n");
}

bool Debugger::proxy_connected() const { return this->connected_to_proxy; }

void Debugger::disconnect_proxy() const {
    if (!this->proxy_connected()) {
        return;
    }
    // TODO close file
    this->supervisor_mutex->unlock();
    this->supervisor->thread.join();
}

void Debugger::updateCallbackmapping(Module *, const char *) {
    // Legacy JSON callback mapping input is intentionally unsupported.
}

// Stop the debugger
void Debugger::stop() {
    if (this->channel != nullptr) {
        this->channel->close();
        this->channel = nullptr;
    }
}

//
void Debugger::pauseRuntime(const Module *m) {
    m->warduino->program_state = WARDUINOpause;
    this->mark = nullptr;
}

bool Debugger::handleUpdateModule(Module *m, uint8_t *data) {
    uint8_t *wasm_data = data + 1;
    const uint32_t wasm_len = read_LEB_32(&wasm_data);
    auto *wasm = static_cast<uint8_t *>(malloc(sizeof(uint8_t) * wasm_len));
    memcpy(wasm, wasm_data, wasm_len);
    WARDuino *wd = m->warduino;
    wd->update_module(m, wasm, wasm_len);
    return true;
}

bool Debugger::handleUpdateGlobalValue(const Module *m, uint8_t *data) const {
    debug("Global updates: %x\n", *data);
    const uint32_t index = read_LEB_32(&data);

    if (index >= m->global_count) return false;

    debug("Global %u being changed\n", index);
    StackValue *v = m->globals[index]->value;
    constexpr bool decodeType = false;
    deserialiseStackValue(data, decodeType, v);
    debug("Global %u changed to %u\n", index, v->value.uint32);
    return true;
}

bool Debugger::handleUpdateStackValue(const Module *m, uint8_t *bytes) const {
    const uint32_t idx = read_LEB_32(&bytes);
    if (idx >= STACK_SIZE) {
        return false;
    }
    ExecutionContext *ectx = m->warduino->execution_context;
    StackValue *sv = &ectx->stack[idx];
    // ReSharper disable once CppTooWideScopeInitStatement
    constexpr bool decodeType = false;
    if (!deserialiseStackValue(bytes, decodeType, sv)) {
        return false;
    }
    debug("StackValue %" PRIu32 " changed\n", idx);
    return true;
}

bool Debugger::reset(Module *m) {
    m->warduino->reset_module(m);
    instructions_executed = 0;
    instructions_since_full_snapshot = 0;
    debug("Reset WARDuino.\n");
    return true;
}

std::optional<uint32_t> resolve_imported_function(Module *m,
                                                  std::string function_name) {
    for (uint32_t fidx = 0; fidx < m->import_count; fidx++) {
        if (!strcmp(m->functions[fidx].import_field, function_name.c_str())) {
            return fidx;
        }
    }
    return {};
}

std::string read_string(uint8_t **pos) {
    std::string str;
    char c = *(*pos)++;
    while (c != '\0') {
        str += c;
        c = *(*pos)++;
    }
    return str;
}

void Debugger::addOverride(Module *m, uint8_t *interruptData) {
    const std::string primitive_name = read_string(&interruptData);
    const std::optional<uint32_t> fidx =
        resolve_imported_function(m, primitive_name);
    if (!fidx) {
        debug("Cannot override the result for unknown function \"%s\".\n",
              primitive_name.c_str());
        debug("ack%x;0\n", interruptUnsetOverridePinValue);
        return;
    }

    const uint32_t param_count = m->functions[fidx.value()].type->param_count;
    std::vector<uint32_t> key(param_count + 1);
    for (uint32_t i = 0; i < param_count; i++) {
        key[i] = read_B32(&interruptData);
    }
    key[param_count] = fidx.value();

    const uint32_t result = read_B32(&interruptData);
    debug("ack%x;1\n", interruptSetOverridePinValue);
    overrides[key] = result;
}

void Debugger::removeOverride(Module *m, uint8_t *interruptData) {
    const std::string primitive_name = read_string(&interruptData);
    const std::optional<uint32_t> fidx =
        resolve_imported_function(m, primitive_name);
    if (!fidx) {
        debug("Cannot remove override for unknown function \"%s\".\n",
              primitive_name.c_str());
        debug("ack%x;0\n", interruptUnsetOverridePinValue);
        return;
    }

    const uint32_t param_count = m->functions[fidx.value()].type->param_count;
    std::vector<uint32_t> key(param_count + 1);
    for (uint32_t i = 0; i < param_count; i++) {
        key[i] = read_B32(&interruptData);
    }
    key[param_count] = fidx.value();

    if (overrides.erase(key) == 0) {
        debug("ack%x;0\n", interruptUnsetOverridePinValue);
        return;
    }
    debug("ack%x;1\n", interruptUnsetOverridePinValue);
}

bool Debugger::getMockForArgs(Module *m, uint32_t fidx, uint32_t &result) {
    const uint32_t param_count = m->functions[fidx].type->param_count;
    std::vector<uint32_t> key(param_count + 1);
    const ExecutionContext *ectx = m->warduino->execution_context;
    for (uint32_t i = 0; i < param_count; i++) {
        key[i] = ectx->stack[ectx->sp - (param_count - i - 1)].value.uint32;
    }
    key[param_count] = fidx;
    const auto it = overrides.find(key);
    if (it == overrides.end()) {
        return false;
    }
    result = it->second;
    return true;
}

bool Debugger::handleContinueFor(Module *m) {
    if (remaining_instructions < 0) return false;

    if (remaining_instructions == 0) {
        remaining_instructions = -1;
        if (snapshotPolicy == SnapshotPolicy::checkpointing) {
            checkpoint(m);
        }
        this->sendNotification(debug_NotificationType_NOTIFICATION_PAUSED);
        pauseRuntime(m);
        return true;
    }
    remaining_instructions--;
    return false;
}

void Debugger::notifyCompleteStep(Module *m) const {
    // Upon completing a step in checkpointing mode, make a checkpoint.
    if (m->warduino->debugger->getSnapshotPolicy() ==
        SnapshotPolicy::checkpointing) {
        m->warduino->debugger->checkpoint(m);
    }
    this->sendNotification(debug_NotificationType_NOTIFICATION_STEPPED);
}

Debugger::~Debugger() {
    this->disconnect_proxy();
    this->stop();
    delete this->supervisor_mutex;
    delete this->supervisor;
}
