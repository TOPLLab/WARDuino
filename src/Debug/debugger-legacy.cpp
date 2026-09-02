#include "debugger-private.h"

/**
 * Read the change in bytes array.
 *
 * The array should be of the form
 * [0x10, index, ... new function body 0x0b]
 * Where index is the index without imports
 */
bool Debugger::handle_changed_function(const Module *m, uint8_t *bytes) {
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
bool Debugger::handle_changed_local(const Module *m, uint8_t *bytes) const {
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

void Debugger::notify_pushed_event() const {
    this->send_notification(debug_NotificationType_NOTIFICATION_NEW_EVENT);
}

bool Debugger::handle_pushed_event(char *) const { return false; }
void Debugger::free_state(Module *m, uint8_t *interruptData) {
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
                FATAL("free_state: receiving unknown command\n");
        }
    }
    debug("done with first msg\n");
}

bool Debugger::save_state(Module *m, uint8_t *interruptData) {
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
                        this->add_breakpoint(toPhysicalAddress(virtualBP, m));
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
                FATAL("save_state: Received unknown program state\n");
            }
        }
    }
    auto done = *program_state;
    return done == static_cast<uint8_t>(1);
}

uintptr_t Debugger::read_pointer(uint8_t **data) {
    const uint8_t len = (*data)[0];
    uintptr_t bp = 0x0;
    for (size_t i = 0; i < len; i++) {
        bp <<= sizeof(uint8_t) * 8;
        bp |= (*data)[i + 1];
    }
    *data += 1 + len;  // skip pointer
    return bp;
}

void Debugger::update_callback_mapping(Module *, const char *) {
    // Legacy JSON callback mapping input is intentionally unsupported.
}

bool Debugger::handle_update_module(Module *m, uint8_t *data) {
    uint8_t *wasm_data = data + 1;
    const uint32_t wasm_len = read_LEB_32(&wasm_data);
    auto *wasm = static_cast<uint8_t *>(malloc(sizeof(uint8_t) * wasm_len));
    memcpy(wasm, wasm_data, wasm_len);
    WARDuino *wd = m->warduino;
    wd->update_module(m, wasm, wasm_len);
    return true;
}

bool Debugger::handle_update_global_value(const Module *m, uint8_t *data) const {
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

bool Debugger::handle_update_stack_value(const Module *m, uint8_t *bytes) const {
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

