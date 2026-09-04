#include "debugger-detail.h"
#include "debugger-protocol.h"

bool Debugger::check_debug_messages(Module *m, debug_State *program_state) {
    std::optional<DebugMessage> message = get_debug_message();
    if (!message) return false;

    const auto malformed = [this]() {
        send_notification(debug_NotificationType_NOTIFICATION_MALFORMED);
    };
    const auto require_empty = [&message, &malformed]() {
        if (!message->payload.empty()) {
            malformed();
            return false;
        }
        return true;
    };

    switch (message->type) {
        case debug_Command_COMMAND_RUN:
            if (!require_empty()) break;
            handle_interrupt_run(m, program_state);
            send_notification(debug_NotificationType_NOTIFICATION_CONTINUED);
            break;
        case debug_Command_COMMAND_HALT:
            if (!require_empty()) break;
            send_notification(debug_NotificationType_NOTIFICATION_HALTED);
            if (channel != nullptr) channel->close();
            break;
        case debug_Command_COMMAND_PAUSE:
            if (!require_empty()) break;
            pause_runtime(m);
            if (snapshotPolicy == SnapshotPolicy::checkpointing)
                checkpoint(m, true);
            send_notification(debug_NotificationType_NOTIFICATION_PAUSED);
            break;
        case debug_Command_COMMAND_STEP:
            if (!require_empty()) break;
            handle_step(m, program_state);
            break;
        case debug_Command_COMMAND_STEP_OVER:
            if (!require_empty()) break;
            handle_step_over(m, program_state);
            break;
        case debug_Command_COMMAND_ADD_BREAKPOINT:
        case debug_Command_COMMAND_REMOVE_BREAKPOINT: {
            debug_Breakpoint breakpoint = debug_Breakpoint_init_zero;
            if (!decode_payload(message->payload, debug_Breakpoint_fields,
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
                add_breakpoint(address);
            else
                delete_breakpoint(address);
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_CLEAR_BREAKPOINTS: {
            // todo remove all breakpoints on all modules
            break;
        }
        case debug_Command_COMMAND_CONTINUE_FOR: {
            debug_ContinueFor request = debug_ContinueFor_init_zero;
            if (!decode_payload(message->payload, debug_ContinueFor_fields,
                                &request) ||
                request.count == 0) {
                malformed();
                break;
            }
            remaining_instructions = static_cast<int32_t>(request.count);
            *program_state = debug_State_STATE_WARDUINO_RUN;
            send_notification(debug_NotificationType_NOTIFICATION_CONTINUED);
            break;
        }
        case debug_Command_COMMAND_SNAPSHOT: {
            debug_Include request = debug_Include_init_zero;
            std::vector<uint8_t> selected;
            set_decode_callback(&request.fields, &selected);
            if (!decode_payload(message->payload, debug_Include_fields,
                        &request)) {
                malformed();
                break;
            }
            SnapshotSelection selection = 0;
            if (!parse_selection(selected.data(), selected.size(),
                        &selection)) {
                malformed();
                break;
            }
            pause_runtime(m);
            encode_snapshot(m, selection,
                debug_NotificationType_NOTIFICATION_SNAPSHOT);
            break;
        }
        case debug_Command_COMMAND_UPDATE_LOCAL: {
            const auto update = update_value(message->payload);
            ExecutionContext *context = m->warduino->execution_context;
            if (!update ||
                context->fp + static_cast<int>(update->index) > context->sp) {
                malformed();
                break;
            }
            StackValue *value = &context->stack[context->fp + update->index];
            if (!assign_value(update->value, value)) {
                malformed();
                break;
            }
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_GLOBAL: {
            const auto update = update_value(message->payload);
            if (!update || update->index >= m->global_count) {
                malformed();
                break;
            }
            StackValue *value = m->globals[update->index]->value;
            if (!assign_value(update->value, value)) {
                malformed();
                break;
            }
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_STACK: {
            const auto update = update_value(message->payload);
            ExecutionContext *context = m->warduino->execution_context;
            if (!update || update->index > static_cast<uint32_t>(context->sp)) {
                malformed();
                break;
            }
            StackValue *value = &context->stack[update->index];
            if (!assign_value(update->value, value)) {
                malformed();
                break;
            }
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_MODULE: {
            debug_ModuleUpdate update = debug_ModuleUpdate_init_zero;
            std::vector<uint8_t> wasm;
            set_decode_callback(&update.wasm, &wasm);
            if (!decode_payload(message->payload, debug_ModuleUpdate_fields,
                                &update) ||
                wasm.empty()) {
                malformed();
                break;
            }
            auto *copy = static_cast<uint8_t *>(malloc(wasm.size()));
            if (copy == nullptr) {
                send_operation_result(message->type, false);
                break;
            }
            memcpy(copy, wasm.data(), wasm.size());
            m->warduino->update_module(m, copy, wasm.size());
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_FUNCTION: {
            debug_Function update = debug_Function_init_zero;
            std::vector<uint8_t> instructions;
            set_decode_callback(&update.instructions, &instructions);
            if (!decode_payload(message->payload, debug_Function_fields,
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
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_UPDATE_CALLBACKS: {
            debug_CallbackMapping mapping = debug_CallbackMapping_init_zero;
            std::vector<DecodedCallbackEntry> entries;
            mapping.entries.funcs.decode = collect_callback_entries;
            mapping.entries.arg = &entries;
            if (!decode_payload(message->payload, debug_CallbackMapping_fields,
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
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_SET_SNAPSHOT_POLICY: {
            debug_SnapshotPolicyConfig config =
                debug_SnapshotPolicyConfig_init_zero;
            std::vector<uint8_t> selectedState;
            set_decode_callback(&config.selected_state, &selectedState);
            SnapshotSelection selectedMask = 0;
            if (!decode_payload(message->payload,
                                debug_SnapshotPolicyConfig_fields, &config) ||
                config.policy >
                    debug_SnapshotPolicy_SNAPSHOT_POLICY_CHECKPOINTING ||
                !parse_selection(selectedState.data(), selectedState.size(),
                                 &selectedMask)) {
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
                    send_operation_result(message->type, false);
                    break;
                }
                memcpy(checkpoint_state, selectedState.data(),
                       selectedState.size());
            }
            if (snapshotPolicy == SnapshotPolicy::checkpointing)
                checkpoint(m, true);
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_SET_OVERRIDE:
        case debug_Command_COMMAND_REMOVE_OVERRIDE: {
            debug_Override request = debug_Override_init_zero;
            std::vector<uint8_t> nameBytes;
            std::vector<uint32_t> words;
            set_decode_callback(&request.primitive_name, &nameBytes);
            request.argument_words.funcs.decode = collect_words;
            request.argument_words.arg = &words;
            if (!decode_payload(message->payload, debug_Override_fields,
                                &request)) {
                malformed();
                break;
            }
            const auto fidx = find_imported_function(
                m, std::string(nameBytes.begin(), nameBytes.end()));
            if (!fidx ||
                words.size() != m->functions[*fidx].type->param_count) {
                send_operation_result(message->type, false);
                break;
            }
            words.push_back(*fidx);
            if (message->type == debug_Command_COMMAND_SET_OVERRIDE)
                overrides[words] = request.result;
            else if (overrides.erase(words) == 0) {
                send_operation_result(message->type, false);
                break;
            }
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_LOAD_SNAPSHOT: {
            debug_Snapshot state = debug_Snapshot_init_zero;
            if (!decode_payload(message->payload, debug_Snapshot_fields,
                                &state) ||
                !isToPhysicalAddrPossible(state.program_counter, m)) {
                malformed();
                break;
            }
            pause_runtime(m);
            m->warduino->execution_context->pc_ptr =
                toPhysicalAddress(state.program_counter, m);
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_ADD_PROXY:
        case debug_Command_COMMAND_REMOVE_PROXY: {
            debug_FunctionRef reference = debug_FunctionRef_init_zero;
            if (!decode_payload(message->payload, debug_FunctionRef_fields,
                                &reference) ||
                supervisor == nullptr ||
                reference.function_index >= m->function_count) {
                send_operation_result(message->type, false);
                break;
            }
            if (message->type == debug_Command_COMMAND_ADD_PROXY)
                supervisor->registerProxiedCall(reference.function_index);
            else
                supervisor->unregisterProxiedCall(reference.function_index);
            send_operation_result(message->type, true);
            break;
        }
        case debug_Command_COMMAND_PROXY_CALL:
        case debug_Command_COMMAND_INVOKE: {
            debug_RemoteFunctionCall call = debug_RemoteFunctionCall_init_zero;
            std::vector<debug_Value> values;
            call.arguments.funcs.decode = collect_values;
            call.arguments.arg = &values;
            if (!decode_payload(message->payload,
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
                valid &= assign_value(values[index], &arguments[index]);
            if (!valid) {
                delete[] arguments;
                malformed();
                break;
            }
            if (message->type == debug_Command_COMMAND_PROXY_CALL) {
                if (proxy == nullptr) {
                    delete[] arguments;
                    send_operation_result(message->type, false);
                    break;
                }
                proxy->pushRFC(
                    m,
                    new RFC(call.function_index,
                            m->functions[call.function_index].type, arguments));
                break;
            }
            const debug_State current = m->warduino->program_state;
            m->warduino->program_state = debug_State_STATE_WARDUINO_RUN;
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
                result.results.funcs.encode = encode_values;
                result.results.arg =
                    const_cast<std::vector<StackValue> *>(&results);
            } else {
                error.assign(exception, exception + std::strlen(exception));
                result.error.funcs.encode = encode_bytes;
                result.error.arg = &error;
            }
            send_notification(
                debug_NotificationType_NOTIFICATION_REMOTE_FUNCTION_RESULT,
                debug_RemoteFunctionResult_fields, &result);
            break;
        }
        case debug_Command_COMMAND_PROXIFY:
            if (!require_empty()) break;
            proxify();
            send_operation_result(message->type, true);
            break;
        case debug_Command_COMMAND_POP_EVENT:
            if (!require_empty()) break;
            send_operation_result(message->type,
                                  CallbackHandler::resolve_event(true));
            break;
        case debug_Command_COMMAND_PUSH_EVENT: {
            debug_Event event = debug_Event_init_zero;
            std::vector<uint8_t> topic;
            std::vector<uint8_t> payload;
            set_decode_callback(&event.topic, &topic);
            set_decode_callback(&event.payload, &payload);
            if (!decode_payload(message->payload, debug_Event_fields, &event) ||
                topic.empty()) {
                malformed();
                break;
            }
            CallbackHandler::push_event(
                std::string(topic.begin(), topic.end()),
                reinterpret_cast<const char *>(payload.data()), payload.size());
            notify_pushed_event();
            break;
        }
        case debug_Command_COMMAND_RESET:
            if (!require_empty()) break;
            send_operation_result(message->type, reset(m));
            break;
        default:
            malformed();
            break;
    }
    return true;
}
