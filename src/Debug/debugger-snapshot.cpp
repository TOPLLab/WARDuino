#include "debugger-detail.h"
#include "debugger-protocol.h"

void Debugger::dump_heap_info(Module *m) const {
    debug_HeapUsage heap_usage = debug_HeapUsage_init_zero;
    heap_usage.heap_used = m->warduino->get_heap_used();
    send_notification(debug_NotificationType_NOTIFICATION_HEAP_USAGE,
                      debug_HeapUsage_fields, &heap_usage);
}

bool Debugger::parse_selection(const uint8_t *fields, const size_t size,
                               SnapshotSelection *selection) {
    constexpr uint32_t validSections =
        debug_SnapshotSection_SNAPSHOT_SECTION_PC |
        debug_SnapshotSection_SNAPSHOT_SECTION_BREAKPOINTS |
        debug_SnapshotSection_SNAPSHOT_SECTION_CALLSTACK |
        debug_SnapshotSection_SNAPSHOT_SECTION_GLOBALS |
        debug_SnapshotSection_SNAPSHOT_SECTION_TABLE |
        debug_SnapshotSection_SNAPSHOT_SECTION_MEMORY |
        debug_SnapshotSection_SNAPSHOT_SECTION_BRANCH_TABLE |
        debug_SnapshotSection_SNAPSHOT_SECTION_STACK |
        debug_SnapshotSection_SNAPSHOT_SECTION_CALLBACKS |
        debug_SnapshotSection_SNAPSHOT_SECTION_EVENTS |
        debug_SnapshotSection_SNAPSHOT_SECTION_IO |
        debug_SnapshotSection_SNAPSHOT_SECTION_OVERRIDES |
        debug_SnapshotSection_SNAPSHOT_SECTION_HEAP |
        debug_SnapshotSection_SNAPSHOT_SECTION_FUNCTIONS |
        debug_SnapshotSection_SNAPSHOT_SECTION_LOCALS;
    if (size > sizeof(*selection)) return false;

    uint32_t mask = 0;
    for (size_t index = 0; index < size; ++index)
        mask |= static_cast<uint32_t>(fields[index]) << (index * 8);
    if ((mask & ~validSections) != 0) return false;

    *selection = static_cast<SnapshotSelection>(mask);
    return true;
}

bool Debugger::encode_snapshot(
    Module *m, const SnapshotSelection selection,
    const debug_NotificationType notification) const {
    ExecutionContext *ectx = m->warduino->execution_context;
    SnapshotView view{m, this, ectx, &overrides};
    debug_Snapshot state = debug_Snapshot_init_zero;
    std::vector<IOStateElement *> ioState;
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_PC) {
        state.program_counter = toVirtualAddress(ectx->pc_ptr, m);
        switch (m->warduino->program_state) {
            case debug_State_STATE_WARDUINO_RUN:
                state.state = debug_State_STATE_WARDUINO_RUN;
                break;
            case debug_State_STATE_WARDUINO_STEP:
                state.state = debug_State_STATE_WARDUINO_STEP;
                break;
            case debug_State_STATE_PROXY_RUN:
                state.state = debug_State_STATE_PROXY_RUN;
                break;
            case debug_State_STATE_PROXY_HALT:
                state.state = debug_State_STATE_PROXY_HALT;
                break;
            default:
                state.state = debug_State_STATE_WARDUINO_PAUSE;
                break;
        }
    }
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_BREAKPOINTS) {
        state.breakpoints.funcs.encode = encode_breakpoints;
        state.breakpoints.arg = &view;
    }
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_FUNCTIONS) {
        state.functions.funcs.encode = encode_functions;
        state.functions.arg = &view;
    }
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_CALLSTACK) {
        state.callstack.funcs.encode = encode_callstack;
        state.callstack.arg = &view;
    }
    ValueView globals{nullptr, m->global_count, m->globals};
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_GLOBALS) {
        state.globals.funcs.encode = encode_value_range;
        state.globals.arg = &globals;
    }
    ValueView stackValues{ectx->stack,
                          ectx->sp >= 0 ? static_cast<size_t>(ectx->sp + 1) : 0,
                          nullptr};
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_STACK) {
        state.stack.funcs.encode = encode_value_range;
        state.stack.arg = &stackValues;
    }
    ValueView locals = current_locals(ectx);
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_LOCALS) {
        state.has_locals = true;
        state.locals.values.funcs.encode = encode_value_range;
        state.locals.values.arg = &locals;
    }
    Uint32View table{m->table.entries,
                     m->table.entries == nullptr ? 0 : m->table.size};
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_TABLE) {
        state.has_table = true;
        state.table.initial = m->table.initial;
        state.table.maximum = m->table.maximum;
        state.table.entries.funcs.encode = nanopb_encoder::encode_varints;
        state.table.entries.arg = &table;
    }
    const size_t memorySize = static_cast<size_t>(m->memory.pages) * PAGE_SIZE;
    ByteView memory{m->memory.bytes,
                    m->memory.bytes == nullptr ? 0 : memorySize};
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_MEMORY) {
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
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_BRANCH_TABLE) {
        state.branch_table.funcs.encode = nanopb_encoder::encode_varints;
        state.branch_table.arg = &branch;
    }
    const auto &callbacks = CallbackHandler::callback_map();
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_CALLBACKS) {
        state.has_callbacks = true;
        state.callbacks.entries.funcs.encode = encode_callbacks;
        state.callbacks.entries.arg =
            const_cast<CallbackHandler::CallbackMap *>(&callbacks);
    }
    const size_t eventCount = CallbackHandler::event_count();
    EventRangeView events{0, eventCount};
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_EVENTS) {
        state.has_queue = true;
        state.queue.total_count = static_cast<uint32_t>(eventCount);
        state.queue.has_range = true;
        state.queue.range.start = 0;
        state.queue.range.end = static_cast<uint32_t>(eventCount);
        state.queue.events.funcs.encode = encode_events;
        state.queue.events.arg = &events;
    }
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_IO) {
        ioState = m->warduino->interpreter->get_io_state(m);
        state.io.funcs.encode = encode_io_state;
        state.io.arg = &ioState;
    }
    if (selection & debug_SnapshotSection_SNAPSHOT_SECTION_OVERRIDES) {
        state.overrides.funcs.encode = encode_overrides;
        state.overrides.arg = &view;
    }
    const bool sent =
        send_notification(notification, debug_Snapshot_fields, &state);
    for (IOStateElement *entry : ioState) delete entry;
    return sent;
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
        if (checkpointSelection != 0)
            encode_snapshot(m, checkpointSelection,
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

    if (checkpointSelection != 0) {
        // Checkpoints only materialize the requested fields.  Keep their views
        // on this stack through nanopb sizing and encoding.
        ExecutionContext *ectx = m->warduino->execution_context;
        notification.has_snapshot = true;
        if (checkpointSelection & debug_SnapshotSection_SNAPSHOT_SECTION_PC) {
            notification.snapshot.program_counter =
                toVirtualAddress(ectx->pc_ptr, m);
            notification.snapshot.state =
                m->warduino->program_state == debug_State_STATE_WARDUINO_RUN
                    ? debug_State_STATE_WARDUINO_RUN
                    : debug_State_STATE_WARDUINO_PAUSE;
        }
        ValueView globals{nullptr, m->global_count, m->globals};
        /* Globals are read directly while nanopb encodes this checkpoint. */
        if (checkpointSelection &
            debug_SnapshotSection_SNAPSHOT_SECTION_GLOBALS) {
            notification.snapshot.globals.funcs.encode = encode_value_range;
            notification.snapshot.globals.arg = &globals;
        }
        ValueView stack{ectx->stack,
                        ectx->sp >= 0 ? static_cast<size_t>(ectx->sp + 1) : 0,
                        nullptr};
        /* Stack values are read directly while nanopb encodes this checkpoint.
         */
        if (checkpointSelection &
            debug_SnapshotSection_SNAPSHOT_SECTION_STACK) {
            notification.snapshot.stack.funcs.encode = encode_value_range;
            notification.snapshot.stack.arg = &stack;
        }
    }
    send_notification(debug_NotificationType_NOTIFICATION_CHECKPOINT,
                      debug_Checkpoint_fields, &notification);
    instructions_executed = 0;
}
