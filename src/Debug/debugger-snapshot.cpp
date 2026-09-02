#include "debugger-detail.h"
#include "debugger-protocol.h"

std::optional<debug_ValueUpdate> Debugger::update_value(
    const std::vector<uint8_t> &payload) const {
    debug_ValueUpdate update = debug_ValueUpdate_init_zero;
    if (!decode_payload(payload, debug_ValueUpdate_fields, &update) ||
        !update.has_value) {
        return std::nullopt;
    }
    return update;
}

void Debugger::dump(Module *m, bool) const { snapshot(m); }

void Debugger::dump_stack(const Module *m) const {
    const ExecutionContext *ectx = m->warduino->execution_context;
    ValueView values{ectx->stack,
                     ectx->sp >= 0 ? static_cast<size_t>(ectx->sp + 1) : 0,
                     nullptr};
    /* ValueView points directly at the execution stack. */
    debug_Locals locals = debug_Locals_init_zero;
    locals.values.funcs.encode = encode_value_range;
    locals.values.arg = &values;
    send_notification(debug_NotificationType_NOTIFICATION_LOCALS_DUMP,
                     debug_Locals_fields, &locals);
}

void Debugger::dump_breakpoints(Module *) const {}

void Debugger::dump_functions(Module *) const {}

/*
 * {"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s
 */
void Debugger::dump_callstack(Module *) const {}

void Debugger::dump_locals(const Module *m) const {
    ValueView values = current_locals(m->warduino->execution_context);
    debug_Locals locals = debug_Locals_init_zero;
    locals.values.funcs.encode = encode_value_range;
    locals.values.arg = &values;
    send_notification(debug_NotificationType_NOTIFICATION_LOCALS_DUMP,
                     debug_Locals_fields, &locals);
}

void Debugger::dump_events(long start, long size) const {
    const size_t total = CallbackHandler::event_count();
    const size_t first =
        std::min(start < 0 ? size_t{0} : static_cast<size_t>(start), total);
    const size_t count =
        size < 0 ? 0 : std::min(static_cast<size_t>(size), total - first);
    EventRangeView range{first, count};
    debug_EventsQueue queue = debug_EventsQueue_init_zero;
    queue.total_count = static_cast<uint32_t>(total);
    queue.has_range = true;
    queue.range.start = static_cast<uint32_t>(first);
    queue.range.end = static_cast<uint32_t>(first + count);
    queue.events.funcs.encode = encode_events;
    queue.events.arg = &range;
    send_notification(debug_NotificationType_NOTIFICATION_EVENTS_DUMP,
                     debug_EventsQueue_fields, &queue);
}

void Debugger::dump_callback_mapping() const {
    const auto &callbacks = CallbackHandler::callback_map();
    debug_CallbackMapping mapping = debug_CallbackMapping_init_zero;
    mapping.entries.funcs.encode = encode_callbacks;
    mapping.entries.arg =
        const_cast<CallbackHandler::CallbackMap *>(&callbacks);
    send_notification(debug_NotificationType_NOTIFICATION_CALLBACKS_DUMP,
                     debug_CallbackMapping_fields, &mapping);
}

void Debugger::dump_heap_info(Module *) const {}

bool Debugger::parse_selection(const uint8_t *state, const size_t size,
                              SnapshotSelection *selection) {
    *selection = 0;
    for (size_t index = 0; index < size; ++index) {
        if (state[index] < pcState || state[index] > heapState) return false;
        *selection |= static_cast<SnapshotSelection>(1u << (state[index] - 1));
    }
    return true;
}

bool Debugger::encode_snapshot(Module *m, const SnapshotSelection selection,
                              const debug_NotificationType notification) const {
    ExecutionContext *ectx = m->warduino->execution_context;
    SnapshotView view{m, this, ectx, &overrides};
    debug_Snapshot state = debug_Snapshot_init_zero;
    std::vector<IOStateElement *> ioState;
    if (selection & snapshotPc) {
        state.program_counter = toVirtualAddress(ectx->pc_ptr, m);
        switch (m->warduino->program_state) {
            case WARDUINOrun:
                state.state = debug_State_STATE_WARDUINO_RUN;
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
    }
    if (selection & snapshotBreakpoints) {
        state.breakpoints.funcs.encode = encode_breakpoints;
        state.breakpoints.arg = &view;
    }
    if (selection & snapshotFunctions) {
        state.functions.funcs.encode = encode_functions;
        state.functions.arg = &view;
    }
    if (selection & snapshotCallstack) {
        state.callstack.funcs.encode = encode_callstack;
        state.callstack.arg = &view;
    }
    ValueView globals{nullptr, m->global_count, m->globals};
    if (selection & snapshotGlobals) {
        state.globals.funcs.encode = encode_value_range;
        state.globals.arg = &globals;
    }
    ValueView stackValues{ectx->stack,
                          ectx->sp >= 0 ? static_cast<size_t>(ectx->sp + 1) : 0,
                          nullptr};
    if (selection & snapshotStack) {
        state.stack.funcs.encode = encode_value_range;
        state.stack.arg = &stackValues;
    }
    ValueView locals = current_locals(ectx);
    if (selection & snapshotLocals) {
        state.has_locals = true;
        state.locals.values.funcs.encode = encode_value_range;
        state.locals.values.arg = &locals;
    }
    Uint32View table{m->table.entries,
                     m->table.entries == nullptr ? 0 : m->table.size};
    if (selection & snapshotTable) {
        state.has_table = true;
        state.table.initial = m->table.initial;
        state.table.maximum = m->table.maximum;
        state.table.entries.funcs.encode = nanopb_encoder::encode_varints;
        state.table.entries.arg = &table;
    }
    const size_t memorySize = static_cast<size_t>(m->memory.pages) * PAGE_SIZE;
    ByteView memory{m->memory.bytes,
                    m->memory.bytes == nullptr ? 0 : memorySize};
    if (selection & snapshotMemory) {
        state.has_memory = true;
        state.memory.initial = m->memory.initial;
        state.memory.maximum = m->memory.maximum;
        state.memory.pages = m->memory.pages;
        state.memory.bytes.funcs.encode = nanopb_encoder::encode_bytes;
        state.memory.bytes.arg = &memory;
    }
    Uint32View branch{ectx->br_table, ectx->br_table == nullptr
                                          ? size_t{0}
                                          : static_cast<size_t>(BR_TABLE_SIZE)};
    if (selection & snapshotBranchTable) {
        state.branch_table.funcs.encode = nanopb_encoder::encode_varints;
        state.branch_table.arg = &branch;
    }
    const auto &callbacks = CallbackHandler::callback_map();
    if (selection & snapshotCallbacks) {
        state.has_callbacks = true;
        state.callbacks.entries.funcs.encode = encode_callbacks;
        state.callbacks.entries.arg =
            const_cast<CallbackHandler::CallbackMap *>(&callbacks);
    }
    const size_t eventCount = CallbackHandler::event_count();
    EventRangeView events{0, eventCount};
    if (selection & snapshotEvents) {
        state.has_queue = true;
        state.queue.total_count = static_cast<uint32_t>(eventCount);
        state.queue.has_range = true;
        state.queue.range.start = 0;
        state.queue.range.end = static_cast<uint32_t>(eventCount);
        state.queue.events.funcs.encode = encode_events;
        state.queue.events.arg = &events;
    }
    if (selection & snapshotIO) {
        ioState = m->warduino->interpreter->get_io_state(m);
        state.io.funcs.encode = encode_io_state;
        state.io.arg = &ioState;
    }
    if (selection & snapshotOverrides) {
        state.overrides.funcs.encode = encode_overrides;
        state.overrides.arg = &view;
    }
    if (selection & snapshotHeap)
        state.heap_used = m->warduino->get_heap_used();
    const bool sent =
        send_notification(notification, debug_Snapshot_fields, &state);
    for (IOStateElement *entry : ioState) delete entry;
    return sent;
}

void Debugger::snapshot(Module *m) const {
    constexpr SnapshotSelection complete =
        snapshotPc | snapshotBreakpoints | snapshotCallstack | snapshotGlobals |
        snapshotTable | snapshotMemory | snapshotBranchTable | snapshotStack |
        snapshotCallbacks | snapshotEvents | snapshotIO | snapshotOverrides |
        snapshotHeap | snapshotFunctions | snapshotLocals;
    encode_snapshot(m, complete, debug_NotificationType_NOTIFICATION_SNAPSHOT);
}

void Debugger::inspect(Module *m, const uint16_t size,
                       const uint8_t *state) const {
    SnapshotSelection selection = 0;
    if (!parse_selection(state, size, &selection)) {
        send_notification(debug_NotificationType_NOTIFICATION_MALFORMED);
        return;
    }
    encode_snapshot(m, selection, debug_NotificationType_NOTIFICATION_SNAPSHOT);
}

void Debugger::set_snapshot_policy(Module *m, uint8_t *interruptData) {
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
        checkpoint(m, true);
    }
    printf("ack%x\n", interruptSetSnapshotPolicy);
}

std::optional<uint32_t> get_primitive_being_called(Module *m, uint8_t *pc_ptr) {
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

void Debugger::handle_snapshot_policy(Module *m) {
    if (snapshotPolicy == SnapshotPolicy::atEveryInstruction) {
        SnapshotSelection selection = 0;
        if (checkpoint_state != nullptr &&
            parse_selection(checkpoint_state, checkpoint_state_size, &selection))
            encode_snapshot(m, selection,
                           debug_NotificationType_NOTIFICATION_SNAPSHOT);
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

        instructions_executed++;

        ExecutionContext *ectx = m->warduino->execution_context;
        // Store arguments of last primitive call.
        if ((fidx_called = get_primitive_being_called(m, ectx->pc_ptr))) {
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

void Debugger::checkpoint(Module *m, const bool force) {
    if (instructions_executed == 0 && !force) return;

    debug_Checkpoint notification = debug_Checkpoint_init_zero;
    notification.instruction_count = instructions_executed;
    if (fidx_called) {
        notification.has_primitive_call = true;
        notification.primitive_function_index = *fidx_called;
    }

    SnapshotSelection selection = 0;
    if (checkpoint_state != nullptr &&
        parse_selection(checkpoint_state, checkpoint_state_size, &selection) &&
        selection != 0) {
        // Checkpoints only materialize the requested fields.  Keep their views
        // on this stack through nanopb sizing and encoding.
        ExecutionContext *ectx = m->warduino->execution_context;
        notification.has_snapshot = true;
        if (selection & snapshotPc) {
            notification.snapshot.program_counter =
                toVirtualAddress(ectx->pc_ptr, m);
            notification.snapshot.state =
                m->warduino->program_state == WARDUINOrun
                    ? debug_State_STATE_WARDUINO_RUN
                    : debug_State_STATE_WARDUINO_PAUSE;
        }
        ValueView globals{nullptr, m->global_count, m->globals};
        /* Globals are read directly while nanopb encodes this checkpoint. */
        if (selection & snapshotGlobals) {
            notification.snapshot.globals.funcs.encode = encode_value_range;
            notification.snapshot.globals.arg = &globals;
        }
        ValueView stack{ectx->stack,
                        ectx->sp >= 0 ? static_cast<size_t>(ectx->sp + 1) : 0,
                        nullptr};
        /* Stack values are read directly while nanopb encodes this checkpoint.
         */
        if (selection & snapshotStack) {
            notification.snapshot.stack.funcs.encode = encode_value_range;
            notification.snapshot.stack.arg = &stack;
        }
        if (selection & snapshotHeap)
            notification.snapshot.heap_used = m->warduino->get_heap_used();
    }
    send_notification(debug_NotificationType_NOTIFICATION_CHECKPOINT,
                     debug_Checkpoint_fields, &notification);
    instructions_executed = 0;
}

