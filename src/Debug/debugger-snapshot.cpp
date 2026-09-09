#include "debugger-decode.h"
#include "debugger-encode.h"

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

// Nanopb callback fields only store pointers to their source data. Keep the
// views used by a snapshot alive together until the synchronous encode ends.
class SnapshotEncodingContext {
   public:
    SnapshotEncodingContext(
        Module *module, const Debugger *debugger,
        const std::unordered_map<std::vector<uint32_t>, uint32_t,
                                 FNV1aVectorHash> *overrides)
        : module(module),
          context(module->warduino->execution_context),
          view{module, debugger, context, overrides},
          globals{nullptr, module->global_count, module->globals},
          stack{context->stack,
                context->sp >= 0 ? static_cast<size_t>(context->sp + 1) : 0,
                nullptr},
          locals(current_locals(context)),
          table{module->table.entries,
                module->table.entries == nullptr ? 0 : module->table.size},
          memory{module->memory.bytes,
                 module->memory.bytes == nullptr
                     ? 0
                     : static_cast<size_t>(module->memory.pages) * PAGE_SIZE},
          branch{context->br_table, context->br_table == nullptr
                                        ? size_t{0}
                                        : static_cast<size_t>(BR_TABLE_SIZE)},
          events{0, CallbackHandler::event_count()} {}

    SnapshotEncodingContext(const SnapshotEncodingContext &) = delete;
    SnapshotEncodingContext &operator=(const SnapshotEncodingContext &) =
        delete;

    ~SnapshotEncodingContext() {
        for (IOStateElement *entry : ioState) delete entry;
    }

    void populate(debug_Snapshot *state, const SnapshotSelection selection) {
        // Register callbacks only for requested sections. The callbacks stream
        // directly from the VM instead of building a second snapshot image.
        constexpr SnapshotSelection sections[] = {
            debug_SnapshotSection_SNAPSHOT_SECTION_PC,
            debug_SnapshotSection_SNAPSHOT_SECTION_BREAKPOINTS,
            debug_SnapshotSection_SNAPSHOT_SECTION_FUNCTIONS,
            debug_SnapshotSection_SNAPSHOT_SECTION_CALLSTACK,
            debug_SnapshotSection_SNAPSHOT_SECTION_GLOBALS,
            debug_SnapshotSection_SNAPSHOT_SECTION_STACK,
            debug_SnapshotSection_SNAPSHOT_SECTION_LOCALS,
            debug_SnapshotSection_SNAPSHOT_SECTION_TABLE,
            debug_SnapshotSection_SNAPSHOT_SECTION_MEMORY,
            debug_SnapshotSection_SNAPSHOT_SECTION_BRANCH_TABLE,
            debug_SnapshotSection_SNAPSHOT_SECTION_CALLBACKS,
            debug_SnapshotSection_SNAPSHOT_SECTION_EVENTS,
            debug_SnapshotSection_SNAPSHOT_SECTION_IO,
            debug_SnapshotSection_SNAPSHOT_SECTION_OVERRIDES,
        };

        for (const SnapshotSelection section : sections) {
            if (!(selection & section)) continue;

            switch (section) {
                case debug_SnapshotSection_SNAPSHOT_SECTION_PC:
                    state->program_counter =
                        toVirtualAddress(context->pc_ptr, module);
                    state->state = module->warduino->program_state;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_BREAKPOINTS:
                    state->breakpoints.funcs.encode = encode_breakpoints;
                    state->breakpoints.arg = &view;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_FUNCTIONS:
                    state->functions.funcs.encode = encode_functions;
                    state->functions.arg = &view;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_CALLSTACK:
                    state->callstack.funcs.encode = encode_callstack;
                    state->callstack.arg = &view;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_GLOBALS:
                    state->globals.funcs.encode = encode_value_range;
                    state->globals.arg = &globals;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_STACK:
                    state->stack.funcs.encode = encode_value_range;
                    state->stack.arg = &stack;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_LOCALS:
                    state->has_locals = true;
                    state->locals.values.funcs.encode = encode_value_range;
                    state->locals.values.arg = &locals;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_TABLE:
                    state->has_table = true;
                    state->table.initial = module->table.initial;
                    state->table.maximum = module->table.maximum;
                    state->table.entries.funcs.encode =
                        nanopb_encoder::encode_varints;
                    state->table.entries.arg = &table;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_MEMORY:
                    state->has_memory = true;
                    state->memory.initial = module->memory.initial;
                    state->memory.maximum = module->memory.maximum;
                    state->memory.pages = module->memory.pages;
                    state->memory.bytes.funcs.encode =
                        nanopb_encoder::encode_bytes;
                    state->memory.bytes.arg = &memory;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_BRANCH_TABLE:
                    state->branch_table.funcs.encode =
                        nanopb_encoder::encode_varints;
                    state->branch_table.arg = &branch;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_CALLBACKS:
                    state->has_callbacks = true;
                    state->callbacks.entries.funcs.encode = encode_callbacks;
                    // Encoding is read-only, but nanopb exposes callback
                    // arguments as void* rather than const void*.
                    state->callbacks.entries.arg =
                        const_cast<CallbackHandler::CallbackMap *>(
                            &CallbackHandler::callback_map());
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_EVENTS:
                    state->has_queue = true;
                    state->queue.total_count =
                        static_cast<uint32_t>(events.size);
                    state->queue.has_range = true;
                    state->queue.range.start = 0;
                    state->queue.range.end = static_cast<uint32_t>(events.size);
                    state->queue.events.funcs.encode = encode_events;
                    state->queue.events.arg = &events;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_IO:
                    ioState =
                        module->warduino->interpreter->get_io_state(module);
                    state->io.funcs.encode = encode_io_state;
                    state->io.arg = &ioState;
                    break;
                case debug_SnapshotSection_SNAPSHOT_SECTION_OVERRIDES:
                    state->overrides.funcs.encode = encode_overrides;
                    state->overrides.arg = &view;
                    break;
                default:
                    break;
            }
        }
    }

   private:
    Module *module;
    ExecutionContext *context;
    SnapshotView view;
    ValueView globals;
    ValueView stack;
    ValueView locals;
    Uint32View table;
    ByteView memory;
    Uint32View branch;
    EventRangeView events;
    std::vector<IOStateElement *> ioState;
};

bool Debugger::send_snapshot(Module *m, const SnapshotSelection selection,
                             const debug_NotificationType notification) const {
    SnapshotEncodingContext context{m, this, &overrides};
    debug_Snapshot state = debug_Snapshot_init_zero;
    context.populate(&state, selection);
    return send_notification(notification, debug_Snapshot_fields, &state);
}

bool Debugger::load_snapshot(Module *m, const std::vector<uint8_t> &payload) {
    // Snapshot restoration has not been implemented yet. Keep the command
    // explicitly rejected until it can validate and atomically restore every
    // runtime section, rather than leaving its declared handler undefined.
    (void)m;
    (void)payload;
    return false;
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
        send_snapshot(m, full_snapshot_selection(),
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

        // In tracing mode, insert a full checkpoint on the same cadence as
        // main. UINT32_MAX explicitly disables this behaviour.
        if (hasCheckpointSelection) {
            if (checkpointInterval != UINT32_MAX &&
                instructions_since_full_snapshot >= checkpointInterval) {
                checkpoint(m, true, true);
                instructions_since_full_snapshot = 0;
            }
            instructions_since_full_snapshot++;
        }

        instructions_executed++;

        ExecutionContext *ectx = m->warduino->execution_context;
        // Store arguments of last primitive call in declaration order.
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

void Debugger::checkpoint(Module *m, const bool force, const bool full) {
    if (instructions_executed == 0 && !force) return;

    debug_Checkpoint notification = debug_Checkpoint_init_zero;
    notification.instruction_count = instructions_executed;

    const SnapshotSelection selection = full || !hasCheckpointSelection
                                            ? full_snapshot_selection()
                                            : checkpointSelection;
    SnapshotEncodingContext snapshot{m, this, &overrides};
    notification.has_snapshot = true;
    snapshot.populate(&notification.snapshot, selection);

    Uint32ValueView arguments{nullptr, 0};
    ReverseUint32ValueView results{nullptr, 0};
    if (fidx_called) {
        const Type *type = m->functions[*fidx_called].type;
        notification.has_primitive_call = true;
        notification.primitive_function_index = *fidx_called;

        arguments = {prim_args, type->param_count};
        notification.arguments.funcs.encode = encode_uint32_range;
        notification.arguments.arg = &arguments;

        ExecutionContext *ectx = m->warduino->execution_context;
        results = {ectx->stack + ectx->sp, type->result_count};
        notification.results.funcs.encode = encode_reverse_uint32_stack_range;
        notification.results.arg = &results;
    }

    send_notification(debug_NotificationType_NOTIFICATION_CHECKPOINT,
                      debug_Checkpoint_fields, &notification);
    instructions_executed = 0;
}
