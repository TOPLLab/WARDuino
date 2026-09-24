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

namespace {

struct DecodedFunction {
    uint32_t index;
    uint32_t start;
    uint32_t end;
    std::vector<uint8_t> instructions;
};

struct DecodedFrame {
    uint32_t type;
    uint32_t functionIndex;
    uint32_t stackPointer;
    uint32_t framePointer;
    uint32_t start;
    uint32_t returnAddress;
};

struct DecodedEvent {
    std::string topic;
    std::string payload;
};

struct DecodedOverride {
    std::string primitiveName;
    std::vector<uint32_t> argumentWords;
    uint32_t result;
};

struct DecodedSnapshot {
    uint32_t programCounter;
    debug_State state;
    std::vector<uint32_t> breakpoints;
    std::vector<DecodedFunction> functions;
    std::vector<DecodedFrame> callstack;
    std::vector<debug_Value> globals;
    std::vector<debug_Value> stack;
    std::vector<debug_Value> locals;
    std::vector<uint32_t> tableEntries;
    uint32_t tableInitial;
    uint32_t tableMaximum;
    std::vector<uint8_t> memoryBytes;
    uint32_t memoryInitial;
    uint32_t memoryMaximum;
    uint32_t memoryPages;
    std::vector<uint32_t> branchTable;
    std::vector<DecodedCallbackEntry> callbacks;
    std::vector<DecodedEvent> events;
    std::vector<IOStateElement> ioState;
    std::vector<DecodedOverride> overrides;
};

bool collect_snapshot_function(pb_istream_t *stream, const pb_field_iter_t *,
                               void **arg) {
    auto *out = static_cast<std::vector<DecodedFunction> *>(*arg);
    debug_Function function = debug_Function_init_zero;
    std::vector<uint8_t> instructions;
    set_decode_callback(&function.instructions, &instructions);
    if (!pb_decode(stream, debug_Function_fields, &function) ||
        !function.has_range)
        return false;
    out->push_back({function.function_index, function.range.start,
                    function.range.end, std::move(instructions)});
    return true;
}

bool collect_snapshot_frame(pb_istream_t *stream, const pb_field_iter_t *,
                            void **arg) {
    auto *out = static_cast<std::vector<DecodedFrame> *>(*arg);
    debug_CallstackEntry frame = debug_CallstackEntry_init_zero;
    if (!pb_decode(stream, debug_CallstackEntry_fields, &frame)) return false;
    out->push_back({frame.type, frame.function_index, frame.stack_pointer,
                    frame.frame_pointer, frame.start, frame.return_address});
    return true;
}

bool collect_snapshot_event(pb_istream_t *stream, const pb_field_iter_t *,
                            void **arg) {
    auto *out = static_cast<std::vector<DecodedEvent> *>(*arg);
    debug_Event event = debug_Event_init_zero;
    std::vector<uint8_t> topic;
    std::vector<uint8_t> payload;
    set_decode_callback(&event.topic, &topic);
    set_decode_callback(&event.payload, &payload);
    if (!pb_decode(stream, debug_Event_fields, &event)) return false;
    out->push_back({std::string(topic.begin(), topic.end()),
                    std::string(payload.begin(), payload.end())});
    return true;
}

bool collect_snapshot_io_state(pb_istream_t *stream, const pb_field_iter_t *,
                               void **arg) {
    auto *out = static_cast<std::vector<IOStateElement> *>(*arg);
    debug_IOState state = debug_IOState_init_zero;
    std::vector<uint8_t> key;
    set_decode_callback(&state.key, &key);
    if (!pb_decode(stream, debug_IOState_fields, &state)) return false;
    out->push_back(
        {std::string(key.begin(), key.end()), state.output, state.value});
    return true;
}

bool collect_snapshot_override(pb_istream_t *stream, const pb_field_iter_t *,
                               void **arg) {
    auto *out = static_cast<std::vector<DecodedOverride> *>(*arg);
    debug_Override override = debug_Override_init_zero;
    std::vector<uint8_t> primitiveName;
    std::vector<uint32_t> argumentWords;
    set_decode_callback(&override.primitive_name, &primitiveName);
    override.argument_words.funcs.decode = collect_words;
    override.argument_words.arg = &argumentWords;
    if (!pb_decode(stream, debug_Override_fields, &override)) return false;
    out->push_back({std::string(primitiveName.begin(), primitiveName.end()),
                    std::move(argumentWords), override.result});
    return true;
}

bool decode_snapshot(const std::vector<uint8_t> &payload,
                     DecodedSnapshot *out) {
    debug_Snapshot snapshot = debug_Snapshot_init_zero;
    snapshot.breakpoints.funcs.decode = collect_varints;
    snapshot.breakpoints.arg = &out->breakpoints;
    snapshot.functions.funcs.decode = collect_snapshot_function;
    snapshot.functions.arg = &out->functions;
    snapshot.callstack.funcs.decode = collect_snapshot_frame;
    snapshot.callstack.arg = &out->callstack;
    snapshot.globals.funcs.decode = collect_values;
    snapshot.globals.arg = &out->globals;
    snapshot.stack.funcs.decode = collect_values;
    snapshot.stack.arg = &out->stack;
    snapshot.locals.values.funcs.decode = collect_values;
    snapshot.locals.values.arg = &out->locals;
    snapshot.table.entries.funcs.decode = collect_varints;
    snapshot.table.entries.arg = &out->tableEntries;
    set_decode_callback(&snapshot.memory.bytes, &out->memoryBytes);
    snapshot.branch_table.funcs.decode = collect_varints;
    snapshot.branch_table.arg = &out->branchTable;
    snapshot.callbacks.entries.funcs.decode = collect_callback_entries;
    snapshot.callbacks.entries.arg = &out->callbacks;
    snapshot.queue.events.funcs.decode = collect_snapshot_event;
    snapshot.queue.events.arg = &out->events;
    snapshot.io.funcs.decode = collect_snapshot_io_state;
    snapshot.io.arg = &out->ioState;
    snapshot.overrides.funcs.decode = collect_snapshot_override;
    snapshot.overrides.arg = &out->overrides;

    if (!decode_payload(payload, debug_Snapshot_fields, &snapshot))
        return false;

    // Nested messages have presence bits. Together with the exact-size checks
    // below, these reject selected/partial snapshots.
    if (!snapshot.has_locals || !snapshot.has_queue ||
        !snapshot.has_callbacks || !snapshot.has_table ||
        !snapshot.has_memory || !snapshot.queue.has_range ||
        snapshot.queue.range.start != 0 ||
        snapshot.queue.range.end != snapshot.queue.total_count ||
        snapshot.queue.total_count != out->events.size())
        return false;

    out->programCounter = snapshot.program_counter;
    out->state = snapshot.state;
    out->tableInitial = snapshot.table.initial;
    out->tableMaximum = snapshot.table.maximum;
    out->memoryInitial = snapshot.memory.initial;
    out->memoryMaximum = snapshot.memory.maximum;
    out->memoryPages = snapshot.memory.pages;
    return true;
}

bool values_are_valid(const std::vector<debug_Value> &values) {
    for (const debug_Value &value : values) {
        StackValue converted{};
        if (!assign_value(value, &converted)) return false;
    }
    return true;
}

bool validate_snapshot(const DecodedSnapshot &snapshot, Module *module) {
    if (snapshot.state > debug_State_STATE_WARDUINO_INIT ||
        !isToPhysicalAddrPossible(snapshot.programCounter, module) ||
        snapshot.functions.size() !=
            module->function_count - module->import_count ||
        snapshot.globals.size() != module->global_count ||
        snapshot.stack.size() > STACK_SIZE ||
        snapshot.callstack.size() > CALLSTACK_SIZE ||
        snapshot.branchTable.size() != BR_TABLE_SIZE ||
        snapshot.memoryBytes.size() !=
            static_cast<size_t>(snapshot.memoryPages) * PAGE_SIZE ||
        !values_are_valid(snapshot.globals) ||
        !values_are_valid(snapshot.stack) || !values_are_valid(snapshot.locals))
        return false;

    std::vector<bool> seenFunctions(module->function_count, false);
    for (const DecodedFunction &function : snapshot.functions) {
        if (function.index < module->import_count ||
            function.index >= module->function_count ||
            seenFunctions[function.index] ||
            !isToPhysicalAddrPossible(function.start, module) ||
            !isToPhysicalAddrPossible(function.end, module) ||
            function.end < function.start ||
            function.instructions.size() !=
                static_cast<size_t>(function.end - function.start + 1))
            return false;
        seenFunctions[function.index] = true;
    }

    for (const uint32_t address : snapshot.breakpoints)
        if (!isToPhysicalAddrPossible(address, module)) return false;

    for (const DecodedFrame &frame : snapshot.callstack) {
        if (frame.type == 0) {
            if (frame.functionIndex >= module->function_count) return false;
        } else if (frame.type != 0xfe && frame.type != 0xff &&
                   !isToPhysicalAddrPossible(frame.start, module)) {
            return false;
        }
        if (frame.returnAddress != 0 &&
            !isToPhysicalAddrPossible(frame.returnAddress, module))
            return false;
    }

    for (const DecodedOverride &override : snapshot.overrides) {
        const auto function =
            find_imported_function(module, override.primitiveName);
        if (!function || override.argumentWords.size() !=
                             module->functions[*function].type->param_count)
            return false;
    }
    return true;
}

Block *restore_frame_block(const DecodedFrame &frame, Module *module) {
    if (frame.type == 0) return &module->functions[frame.functionIndex];
    if (frame.type == 0xfe || frame.type == 0xff) {
        auto *guard = static_cast<Block *>(malloc(sizeof(Block)));
        if (guard == nullptr) return nullptr;
        guard->block_type = frame.type;
        guard->type = nullptr;
        guard->local_value_type = nullptr;
        guard->start_ptr = nullptr;
        guard->end_ptr = nullptr;
        guard->else_ptr = nullptr;
        guard->export_name = nullptr;
        guard->import_field = nullptr;
        guard->import_module = nullptr;
        guard->func_ptr = nullptr;
        return guard;
    }

    const auto block =
        module->block_lookup.find(toPhysicalAddress(frame.start, module));
    return block == module->block_lookup.end() ? nullptr : block->second;
}

void free_restored_guards(std::vector<Block *> *guards) {
    for (Block *guard : *guards) {
        if (guard != nullptr &&
            (guard->block_type == 0xfe || guard->block_type == 0xff))
            free(guard);
    }
    guards->clear();
}

bool apply_snapshot(
    Module *module, const DecodedSnapshot &snapshot,
    std::unordered_map<std::vector<uint32_t>, uint32_t, FNV1aVectorHash>
        *overrides,
    std::unordered_map<uint32_t, std::vector<uint8_t>> *functionBodies,
    std::set<uint8_t *> *breakpoints) {
    std::vector<Block *> frameBlocks;
    frameBlocks.reserve(snapshot.callstack.size());
    for (const DecodedFrame &frame : snapshot.callstack) {
        Block *block = restore_frame_block(frame, module);
        if (block == nullptr) {
            free_restored_guards(&frameBlocks);
            return false;
        }
        frameBlocks.push_back(block);
    }

    uint32_t *tableEntries = nullptr;
    if (!snapshot.tableEntries.empty()) {
        tableEntries = static_cast<uint32_t *>(
            malloc(snapshot.tableEntries.size() * sizeof(uint32_t)));
        if (tableEntries == nullptr) {
            free_restored_guards(&frameBlocks);
            return false;
        }
        memcpy(tableEntries, snapshot.tableEntries.data(),
               snapshot.tableEntries.size() * sizeof(uint32_t));
    }

    uint8_t *memoryBytes = nullptr;
    if (!snapshot.memoryBytes.empty()) {
        memoryBytes =
            static_cast<uint8_t *>(malloc(snapshot.memoryBytes.size()));
        if (memoryBytes == nullptr) {
            free(tableEntries);
            free_restored_guards(&frameBlocks);
            return false;
        }
        memcpy(memoryBytes, snapshot.memoryBytes.data(),
               snapshot.memoryBytes.size());
    }

    ExecutionContext *context = module->warduino->execution_context;
    for (int index = 0; index <= context->csp; ++index) {
        Block *block = context->callstack[index].block;
        if (block != nullptr &&
            (block->block_type == 0xfe || block->block_type == 0xff))
            free(block);
    }

    for (const DecodedFunction &function : snapshot.functions) {
        memcpy(module->bytes + function.start, function.instructions.data(),
               function.instructions.size());
        Block &target = module->functions[function.index];
        target.start_ptr = module->bytes + function.start;
        target.end_ptr = module->bytes + function.end;
        target.br_ptr = target.end_ptr;
        functionBodies->erase(function.index);
    }

    free(module->table.entries);
    module->table.entries = tableEntries;
    module->table.size = static_cast<uint32_t>(snapshot.tableEntries.size());
    module->table.initial = snapshot.tableInitial;
    module->table.maximum = snapshot.tableMaximum;

    free(module->memory.bytes);
    module->memory.bytes = memoryBytes;
    module->memory.pages = snapshot.memoryPages;
    module->memory.initial = snapshot.memoryInitial;
    module->memory.maximum = snapshot.memoryMaximum;

    for (size_t index = 0; index < snapshot.globals.size(); ++index)
        assign_value(snapshot.globals[index], module->globals[index]->value);
    for (size_t index = 0; index < snapshot.stack.size(); ++index)
        assign_value(snapshot.stack[index], &context->stack[index]);
    context->sp = static_cast<int>(snapshot.stack.size()) - 1;
    memcpy(context->br_table, snapshot.branchTable.data(),
           snapshot.branchTable.size() * sizeof(uint32_t));

    context->csp = static_cast<int>(snapshot.callstack.size()) - 1;
    context->fp = -1;
    for (size_t index = 0; index < snapshot.callstack.size(); ++index) {
        const DecodedFrame &source = snapshot.callstack[index];
        Frame &target = context->callstack[index];
        target.block = frameBlocks[index];
        target.sp = static_cast<int>(source.stackPointer);
        target.fp = static_cast<int>(source.framePointer);
        target.ra_ptr = source.returnAddress == 0
                            ? nullptr
                            : toPhysicalAddress(source.returnAddress, module);
        target.module = module;
        if (source.type == 0) context->fp = target.sp + 1;
    }
    context->pc_ptr = toPhysicalAddress(snapshot.programCounter, module);
    context->current_module = module;

    breakpoints->clear();
    for (const uint32_t address : snapshot.breakpoints)
        breakpoints->insert(toPhysicalAddress(address, module));

    CallbackHandler::clear_callbacks();
    for (const DecodedCallbackEntry &entry : snapshot.callbacks)
        for (const uint32_t index : entry.indexes)
            CallbackHandler::add_callback(Callback(module, entry.topic, index));

    CallbackHandler::clear_events();
    for (const DecodedEvent &event : snapshot.events)
        CallbackHandler::push_event(
            event.topic, event.payload.data(),
            static_cast<unsigned int>(event.payload.size()));

    overrides->clear();
    for (const DecodedOverride &override : snapshot.overrides) {
        const uint32_t function =
            *find_imported_function(module, override.primitiveName);
        std::vector<uint32_t> key = override.argumentWords;
        key.push_back(function);
        (*overrides)[std::move(key)] = override.result;
    }

    module->warduino->interpreter->restore_external_state(module,
                                                          snapshot.ioState);
    module->warduino->program_state = snapshot.state;
    return true;
}

}  // namespace

bool Debugger::load_snapshot(Module *m, const std::vector<uint8_t> &payload) {
    DecodedSnapshot snapshot;
    const bool loaded =
        m != nullptr && decode_snapshot(payload, &snapshot) &&
        validate_snapshot(snapshot, m) &&
        apply_snapshot(m, snapshot, &overrides, &functionBodies, &breakpoints);
    if (loaded) {
        instructions_executed = 0;
        instructions_since_full_snapshot = 0;
    }
    return loaded;
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
