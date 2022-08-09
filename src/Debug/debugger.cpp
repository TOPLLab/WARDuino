#include "debugger.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

#include "../Memory/mem.h"
#include "../Utils//util.h"
#include "../Utils/macros.h"

// Debugger

Debugger::Debugger(int address) { this->channel = new Channel(address); }

// Public methods

void Debugger::setChannel(int address) {
    delete this->channel;
    this->channel = new Channel(address);
}

void Debugger::addDebugMessage(size_t len, const uint8_t *buff) {
    auto coded_input = new google::protobuf::io::CodedInputStream(buff, len);
    auto *message = new debug::DebugMessage();
    bool success = message->ParseFromCodedStream(coded_input);
    if (success && coded_input->ConsumedEntireMessage() &&
        this->wellformed(message)) {
        this->debugMessages.push_back(message);
    } else {
        dbg_info("Debug message is not well formed. Discarded...\n");
        delete message;
        // TODO handle error
    }
    delete coded_input;
}

bool Debugger::wellformed(const debug::DebugMessage *message) const {
    bool wellformed = true;
    switch (message->command()) {
        case debug::run:
        case debug::halt:
        case debug::pause:
        case debug::step:
        case debug::dump:
        case debug::dumplocals:
        case debug::snapshot:
        case debug::dumpcallbacks:
        case debug::proxify:
        case debug::popevent:
            break;
        case debug::bpadd:
        case debug::bprem:
            wellformed =
                message->has_payload() && message->payload().has_breakpoint();
            break;
        case debug::dumpevents:
            wellformed =
                !message->has_payload() || message->payload().has_range();
            break;
        case debug::updatefunc:
            wellformed =
                message->has_payload() && message->payload().has_function();
            break;
        case debug::updatelocal:
            wellformed =
                message->has_payload() && message->payload().has_locals();
            break;
        case debug::updatecallbacks:
            wellformed =
                message->has_payload() && message->payload().has_callbacks();
            break;
        case debug::loadsnapshot:
            wellformed =
                message->has_payload() && message->payload().has_snapshot();
            break;
        case debug::proxyadd:
        case debug::proxyrem:
            wellformed =
                message->has_payload() && message->payload().has_function();
            break;
        case debug::proxycall:
            wellformed =
                message->has_payload() && message->payload().has_call();
            break;
        case debug::pushevent:
            wellformed =
                message->has_payload() && message->payload().has_event();
            break;
        default:
            wellformed = false;
    }
    return wellformed;
}

debug::DebugMessage *Debugger::getDebugMessage() {
    if (!this->debugMessages.empty()) {
        debug::DebugMessage *message = this->debugMessages.front();
        this->debugMessages.pop_front();
        return message;
    } else {
        return nullptr;
    }
}

void Debugger::addBreakpoint(uint8_t *loc) { this->breakpoints.insert(loc); }

void Debugger::deleteBreakpoint(uint8_t *loc) { this->breakpoints.erase(loc); }

bool Debugger::isBreakpoint(uint8_t *loc) {
    return this->breakpoints.find(loc) != this->breakpoints.end();
}

void Debugger::notifyBreakpoint(uint8_t *pc_ptr) const {
    this->channel->write("AT %p!\n", (void *)pc_ptr);
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
bool Debugger::checkDebugMessages(Module *m, debug::State *program_state) {
    debug::DebugMessage *message = this->getDebugMessage();
    if (message == nullptr) {
        fflush(stdout);
        return false;
    }

    // TODO replace notifications with debug::acknowledgement
    //    this->channel->write("Interrupt: %x\n", message->command());

    switch (message->command()) {
        case debug::run:
            this->handleInterruptRUN(m, program_state);
            break;
        case debug::halt:
            this->channel->write("STOP!\n");
            exit(0);
        case debug::pause:
            *program_state = debug::WARDUINOpause;
            this->channel->write("PAUSE!\n");
            break;
        case debug::step:
            this->channel->write("STEP!\n");
            *program_state = debug::WARDUINOstep;
            this->skipBreakpoint = m->pc_ptr;
            break;
        case debug::bpadd:  // Breakpoint
        case debug::bprem:  // Breakpoint remove
            this->handleInterruptBP(message->payload().breakpoint());  // TODO
            break;
        case debug::dump:
            *program_state = debug::WARDUINOpause;
            this->dump(m);
            break;
        case debug::dumplocals:
            handleInterruptDumplocals(m);
            break;
        case debug::snapshot:
            *program_state = debug::WARDUINOpause;
            this->dump(m, true);
            break;
        case debug::updatefunc:
            this->channel->write("CHANGE function!\n");
            Debugger::handleChangedFunction(m, message->payload().function());
            //  do not free(interruptData);
            // we need it to run that code
            // TODO: free double replacements
            break;
        case debug::updatelocal:
            this->channel->write("CHANGE local!\n");
            this->handleChangedLocal(m, message->payload().locals());
            break;
        case debug::loadsnapshot:
            this->loadState(m, message->payload().snapshot());
            break;
        case debug::proxycall: {
            this->handleProxyCall(m, program_state, message->payload().call());
        } break;
        case debug::proxyadd: {
            m->warduino->debugger->supervisor->registerProxiedCall(
                message->payload().function().fidx());
        } break;
        case debug::proxyrem: {
            m->warduino->debugger->supervisor->unregisterProxiedCall(
                message->payload().function().fidx());
        } break;
        case debug::proxify: {
            dbg_info("Converting to proxy settings.\n");
            this->proxify();
            break;
        }
        case debug::dumpevents:
            handleInterruptDumpevents(message);
            break;
        case debug::popevent:
            CallbackHandler::resolve_event(true);
            break;
        case debug::pushevent:
            this->handlePushedEvent(message->payload().event());
            break;
        case debug::updatecallbacks:
            Debugger::updateCallbackmapping(m, message->payload().callbacks());
            break;
        case debug::dumpcallbacks:
            this->dumpCallbackmapping();
            break;
        default:
            // handle later
            this->channel->write("COULD not parse interrupt data!\n");
            break;
    }
    delete message;
    fflush(stdout);
    return true;
}

void Debugger::handleInterruptDumpevents(
    const debug::DebugMessage *message) const {
    debug::EventsQueue *queue = captureEventsQueue(message->payload().range());
    channel->write(queue->SerializeAsString().c_str());
    delete queue;
}

void Debugger::handleInterruptDumplocals(Module *m) const {
    m->warduino->program_state = debug::WARDUINOpause;
    debug::Locals *locals = captureLocals(m);
    channel->write(locals->SerializeAsString().c_str());
    delete locals;
}

void Debugger::loadState(Module *m, const debug::Snapshot &snapshot) {
    m->warduino->program_state = debug::WARDUINOpause;
    CallbackHandler::manual_event_resolution = true;
    dbg_info("loading: set event resolution... on\n");
    // TODO implement (this->saveState)
    printf("loading: program counter... %i\n", snapshot.program_counter());
    printf("loading: running state... %i\n", snapshot.state());
    printf("loading: breakpoint count... %i\n", snapshot.breakpoints_size());
    for (int i = 0; i < snapshot.functions_size(); i++) {
        printf("loading: function... %i\n", snapshot.functions(i).fidx());
    }
    for (int i = 0; i < snapshot.callstack_size(); i++) {
        printf("loading: callstack entry... (type %i)\n",
               snapshot.callstack(i).type());
    }
    printf("loading: local count... %i\n", snapshot.locals().count());
}

// Private methods
void Debugger::printValue(StackValue *v, uint32_t idx, bool end = false) const {
    char buff[256];

    switch (v->value_type) {
        case I32:
            snprintf(buff, 255, R"("type":"i32","value":%)" PRIi32,
                     v->value.uint32);
            break;
        case I64:
            snprintf(buff, 255, R"("type":"i64","value":%)" PRIi64,
                     v->value.uint64);
            break;
        case F32:
            snprintf(buff, 255, R"("type":"F32","value":%.7f)", v->value.f32);
            break;
        case F64:
            snprintf(buff, 255, R"("type":"F64","value":%.7f)", v->value.f64);
            break;
        default:
            snprintf(buff, 255, R"("type":"%02x","value":"%)" PRIx64 "\"",
                     v->value_type, v->value.uint64);
    }
    this->channel->write(R"({"idx":%d,%s}%s)", idx, buff, end ? "" : ",");
}

uint8_t *Debugger::findOpcode(Module *m, Block *block) {
    auto find =
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

void Debugger::handleInterruptRUN(Module *m, debug::State *program_state) {
    this->channel->write("GO!\n");
    if (*program_state == debug::WARDUINOpause &&
        this->isBreakpoint(m->pc_ptr)) {
        this->skipBreakpoint = m->pc_ptr;
    }
    *program_state = debug::WARDUINOrun;
}

void Debugger::handleInterruptBP(std::string breakpoint) {
    // TODO: segfault may happen here!
    // TODO implement
    //    uint8_t len = interruptData[1];
    //    uintptr_t bp = 0x0;
    //    for (size_t i = 0; i < len; i++) {
    //        bp <<= sizeof(uint8_t) * 8;
    //        bp |= interruptData[i + 2];
    //    }
    //    auto *bpt = (uint8_t *)bp;
    //    this->channel->write("BP %p!\n", static_cast<void *>(bpt));
    //
    //    if (*interruptData == 0x06) {
    //        this->addBreakpoint(bpt);
    //    } else {
    //        this->deleteBreakpoint(bpt);
    //    }
}

void Debugger::dump(Module *m, bool full) const {
    uint8_t *start = m->bytes;
    debug::Snapshot snapshot = debug::Snapshot();

    // current PC
    snapshot.set_program_counter(m->pc_ptr - start);

    // current running snapshot
    snapshot.set_state((debug::State)m->warduino->program_state);

    this->captureBreakpoints(&snapshot);

    this->captureFunctions(m, &snapshot);

    this->captureCallstack(m, &snapshot);

    std::string message = snapshot.SerializeAsString();
    if (full) {
        debug::Locals *locals = this->captureLocals(m);
        snapshot.set_allocated_locals(locals);
        debug::Range range = debug::Range();
        range.set_start(0);
        range.set_end(CallbackHandler::event_count());
        debug::EventsQueue *queue = this->captureEventsQueue(range);
        snapshot.set_allocated_queue(queue);

        message = snapshot.SerializeAsString();

        delete locals;
        delete queue;
    }

    // send snapshot
    this->channel->write(message.c_str());
}

void Debugger::captureBreakpoints(debug::Snapshot *snapshot) const {
    //    uint8_t *start = m->bytes;  // TODO breakpoints also minus start?
    for (auto bp : this->breakpoints) {
        snapshot->add_breakpoints(reinterpret_cast<const char *>(bp));
    }
}

void Debugger::captureFunctions(Module *m, debug::Snapshot *snapshot) const {
    uint8_t *start = m->bytes;
    for (size_t i = m->import_count; i < m->function_count; i++) {
        debug::Function *function = snapshot->add_functions();
        function->set_fidx(m->functions[i].fidx);
        debug::Range *range = function->mutable_range();
        range->set_start(m->functions[i].start_ptr - start);
        range->set_end(m->functions[i].end_ptr - start);
    }
}

void Debugger::captureCallstack(Module *m, debug::Snapshot *snapshot) const {
    uint8_t *start = m->bytes;
    for (int i = 0; i <= m->csp; i++) {
        Frame *f = &m->callstack[i];
        debug::CallstackEntry *entry = snapshot->add_callstack();
        entry->set_type(f->block->block_type);
        entry->set_fidx(f->block->fidx);
        entry->set_sp(f->sp);
        entry->set_fp(f->fp);
        entry->set_start(f->block->start_ptr - start);
        entry->set_ra(f->ra_ptr - start);
    }
}

void inflateValue(StackValue *source, int32_t index,
                  debug::Value *destination) {
    char _value_str[256];
    switch (source->value_type) {
        case I32:
            snprintf(_value_str, 255, "%" PRIi32, source->value.uint32);
            destination->set_type(debug::Value_Type_i32);
            break;
        case I64:
            snprintf(_value_str, 255, "%" PRIi64, source->value.uint64);
            destination->set_type(debug::Value_Type_i64);
            break;
        case F32:
            snprintf(_value_str, 255, "%.7f", source->value.f32);
            destination->set_type(debug::Value_Type_f32);
            break;
        case F64:
            snprintf(_value_str, 255, "%.7f", source->value.f64);
            destination->set_type(debug::Value_Type_f64);
            break;
        default:
            snprintf(_value_str, 255, "%" PRIx64, source->value.uint64);
            destination->set_type(debug::Value_Type_any);
    }
    destination->set_value(_value_str);
    destination->set_index(index);
}

debug::Locals *Debugger::captureLocals(Module *m) const {
    int firstFunFramePtr = m->csp;
    while (m->callstack[firstFunFramePtr].block->block_type != 0) {
        firstFunFramePtr--;
        if (firstFunFramePtr < 0) {
            FATAL("Not in a function!");
        }
    }
    Frame *f = &m->callstack[firstFunFramePtr];
    auto locals = new debug::Locals();
    locals->set_count(f->block->local_count);
    for (uint32_t i = 0; i < f->block->local_count; i++) {
        debug::Value *value = locals->add_values();
        auto v = &m->stack[m->fp + i];
        inflateValue(v, i + f->block->type->param_count, value);
    }
    return locals;
}

debug::EventsQueue *Debugger::captureEventsQueue(
    const debug::Range &payload) const {
    bool previous = CallbackHandler::resolving_event;
    CallbackHandler::resolving_event = true;

    // construct response message
    auto queue = new debug::EventsQueue();
    queue->set_count(CallbackHandler::event_count());

    long start = payload.start(), end = payload.end();
    // check for illegal range
    if (start < 0 || (size_t)start > CallbackHandler::event_count() ||
        end < 0) {
        // return only total count and range (0,0)
        debug::Range *range = queue->mutable_range();
        range->set_start(0);
        range->set_end(0);
        CallbackHandler::resolving_event = previous;
        return queue;
    }
    // clip right-side out-of-bounds range
    if (end > queue->count()) {
        end = queue->count();
    }

    // if not full queue, add range to message
    if (!(start == 0 && end == queue->count())) {
        debug::Range *range = queue->mutable_range();
        range->set_start(start);
        range->set_end(end);
    }

    // add events to messages
    std::for_each(CallbackHandler::event_begin() + start,
                  CallbackHandler::event_begin() + end,
                  [&queue](const Event &e) {
                      debug::Event *event = queue->add_events();
                      event->set_topic(e.topic);
                      event->set_payload(e.payload);
                  });

    CallbackHandler::resolving_event = previous;
    return queue;
}

void Debugger::dumpCallbackmapping() const {
    this->channel->write("%s\n", CallbackHandler::dump_callbacks().c_str());
}

/**
 * Read the change in bytes array.
 *
 * The array should be of the form
 * [0x10, index, ... new function body 0x0b]
 * Where index is the index without imports
 */
bool Debugger::handleChangedFunction(Module *m, debug::Function payload) {
    Block *function = &m->functions[m->import_count + payload.fidx()];
    // TODO
}

/**
 * Read change to local
 * @param m
 * @param bytes
 * @return
 */
bool Debugger::handleChangedLocal(Module *m, debug::Locals locals) const {
    //    if (*bytes != interruptUPDATELocal) return false;
    //    uint8_t *pos = bytes + 1;
    //    this->channel->write("Local updates: %x\n", *pos);
    //    uint32_t localId = read_LEB_32(&pos);
    //
    //    this->channel->write("Local %u being changed\n", localId);
    //    auto v = &m->stack[m->fp + localId];
    //    switch (v->value_type) {
    //        case I32:
    //            v->value.uint32 = read_LEB_signed(&pos, 32);
    //            break;
    //        case I64:
    //            v->value.int64 = read_LEB_signed(&pos, 64);
    //            break;
    //        case F32:
    //            memcpy(&v->value.uint32, pos, 4);
    //            break;
    //        case F64:
    //            memcpy(&v->value.uint64, pos, 8);
    //            break;
    //    }
    //    this->channel->write("Local %u changed to %u\n", localId,
    //    v->value.uint32);
    return true;
}

void Debugger::notifyPushedEvent() const {
    this->channel->write("new pushed event");
}

void Debugger::handlePushedEvent(debug::Event payload) const {
    auto *event = new Event(payload.topic(), payload.payload());
    dbg_info("handle pushed event: %s", event->serialized().c_str());
    CallbackHandler::push_event(event);
    this->notifyPushedEvent();
}

enum ReceiveState {
    pcState = 0x01,
    breakpointsState = 0x02,
    callstackState = 0x03,
    globalsState = 0x04,
    tblState = 0x05,
    memState = 0x06,
    brtblState = 0x07,
    stackvalsState = 0x08,
    pcErrorState = 0x09
};

void Debugger::freeState(Module *m, uint8_t *interruptData) {
    debug("freeing the program state\n");
    uint8_t *first_msg = nullptr;
    uint8_t *endfm = nullptr;
    first_msg = interruptData + 1;  // skip interruptRecvState
    endfm = first_msg + read_B32(&first_msg);

    // nullify state
    this->breakpoints.clear();
    m->csp = -1;
    m->sp = -1;
    memset(m->br_table, 0, BR_TABLE_SIZE);

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
                        m->globals = (StackValue *)acalloc(
                            amount, sizeof(StackValue), "globals");
                } else {
                    debug("globals setting existing state to zero\n");
                    for (uint32_t i = 0; i < m->global_count; i++) {
                        debug("decreasing global_count\n");
                        StackValue *sv = &m->globals[i];
                        sv->value_type = 0;
                        sv->value.uint32 = 0;
                    }
                }
                m->global_count = 0;
                break;
            }
            case tblState: {
                debug("receiving table info\n");
                m->table.initial = read_B32(&first_msg);
                m->table.maximum = read_B32(&first_msg);
                uint32_t size = read_B32(&first_msg);
                debug("init %d max %d size %d\n", m->table.initial,
                      m->table.maximum, size);
                if (m->table.size != size) {
                    debug("old table size %d\n", m->table.size);
                    if (m->table.size != 0) free(m->table.entries);
                    m->table.entries = (uint32_t *)acalloc(
                        size, sizeof(uint32_t), "Module->table.entries");
                }
                m->table.size = 0;  // allows to accumulatively add entries
                break;
            }
            case memState: {
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
                m->memory.bytes =
                    (uint8_t *)acalloc(pages * PAGE_SIZE, sizeof(uint32_t),
                                       "Module->memory.bytes");
                m->memory.pages = pages;
                // }
                // else{
                //   //TODO fill memory.bytes with zeros
                // memset(m->memory.bytes, 0, m->memory.pages * PAGE_SIZE) ;
                // }
                break;
            }
            default: {
                FATAL("freeState: receiving unknown command\n");
                break;
            }
        }
    }
    debug("done with first msg\n");
}

bool Debugger::saveState(Module *m, uint8_t *interruptData) {
    uint8_t *program_state = nullptr;
    uint8_t *end_state = nullptr;
    program_state = interruptData + 1;  // skip interruptRecvState
    end_state = program_state + read_B32(&program_state);

    debug("saving program_state\n");
    while (program_state < end_state) {
        switch (*program_state++) {
            case pcState: {  // PC
                m->pc_ptr = (uint8_t *)readPointer(&program_state);
                /* printf("receiving pc %p\n", static_cast<void*>(m->pc_ptr));
                 */
                break;
            }
            case breakpointsState: {  // breakpoints
                uint8_t quantity_bps = *program_state++;
                debug("receiving breakpoints %" PRIu8 "\n", quantity_bps);
                for (size_t i = 0; i < quantity_bps; i++) {
                    auto bp = (uint8_t *)readPointer(&program_state);
                    this->addBreakpoint(bp);
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
                    m->csp += 1;
                    Frame *f = m->callstack + m->csp;
                    f->sp = read_B32_signed(&program_state);
                    f->fp = read_B32_signed(&program_state);
                    f->ra_ptr = (uint8_t *)readPointer(&program_state);
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
                        m->fp = f->sp + 1;
                    } else {
                        debug("non function block\n");
                        auto *block_key =
                            (uint8_t *)readPointer(&program_state);
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
            case globalsState: {  // TODO merge globalsState stackvalsState into
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
                    StackValue *sv = &m->globals[m->global_count++];
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
            case tblState: {
                program_state++;  // for now only funcref
                uint32_t quantity = read_B32(&program_state);
                for (size_t i = 0; i < quantity; i++) {
                    uint32_t ne = read_B32(&program_state);
                    m->table.entries[m->table.size++] = ne;
                }
                break;
            }
            case memState: {
                debug("receiving memory\n");
                uint32_t start = read_B32(&program_state);
                uint32_t limit = read_B32(&program_state);
                debug("memory offsets start=%" PRIu32 " , limit=%" PRIu32 "\n",
                      start, limit);
                if (start > limit) {
                    FATAL("incorrect memory offsets\n");
                }
                uint32_t total_bytes = limit - start + 1;
                uint8_t *mem_end =
                    m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
                debug("will copy #%" PRIu32 " bytes\n", total_bytes);
                if ((m->memory.bytes + start) + total_bytes > mem_end) {
                    FATAL("memory overflow %p > %p\n",
                          static_cast<void *>((m->bytes + start) + total_bytes),
                          static_cast<void *>(mem_end));
                }
                memcpy(m->memory.bytes + start, program_state, total_bytes + 1);
                for (auto i = start; i <= (start + total_bytes - 1); i++) {
                    debug("GOT byte idx %" PRIu32 " =%" PRIu8 "\n", i,
                          m->memory.bytes[i]);
                }
                debug("outside the out\n");
                program_state += total_bytes;
                break;
            }
            case brtblState: {
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
                    m->br_table[idx] = el;
                }
                break;
            }
            case stackvalsState: {
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
                    m->sp += 1;
                    StackValue *sv = &m->stack[m->sp];
                    sv->value.uint64 = 0;  // init whole union to 0
                    size_t qb = type_index == 0 || type_index == 2 ? 4 : 8;
                    sv->value_type = valtypes[type_index];
                    memcpy(&sv->value, program_state, qb);
                    program_state += qb;
                }
                break;
            }
            default: {
                FATAL("saveState: Received unknown program state\n");
            }
        }
    }
    auto done = (uint8_t)*program_state;
    return done == (uint8_t)1;
}

uintptr_t Debugger::readPointer(uint8_t **data) {
    uint8_t len = (*data)[0];
    uintptr_t bp = 0x0;
    for (size_t i = 0; i < len; i++) {
        bp <<= sizeof(uint8_t) * 8;
        bp |= (*data)[i + 1];
    }
    *data += 1 + len;  // skip pointer
    return bp;
}

void Debugger::proxify() {
    WARDuino::instance()->program_state = debug::PROXYhalt;
    this->proxy = new Proxy();  // TODO delete
}

void Debugger::handleProxyCall(Module *m, debug::State *program_state,
                               const debug::RFC &payload) {
    uint32_t fidx = payload.fidx();
    dbg_info("proxycall: func %" PRIu32 "\n", fidx);

    Block *func = &m->functions[fidx];
    StackValue *args = this->readRFCArgs(payload);
    dbg_trace("proxycall: enqueuing callee... %" PRIu32 "\n", func->fidx);

    auto *rfc = new RFC(fidx, func->type, args);
    this->proxy->pushRFC(m, rfc);

    *program_state = debug::PROXYrun;
    dbg_trace("proxycall: setting running state... ProxyRun\n");
}

RFC *Debugger::topProxyCall() {
    if (proxy == nullptr) {
        return nullptr;
    }
    return this->proxy->topRFC();
}

void Debugger::sendProxyCallResult(Module *m) {
    if (proxy == nullptr) {
        return;
    }
    this->proxy->returnResult(m);
}

bool Debugger::isProxied(uint32_t fidx) const {
    return this->supervisor != nullptr && this->supervisor->isProxied(fidx);
}

void Debugger::startProxySupervisor(int socket) {
    this->connected_to_proxy = true;
    pthread_mutex_init(&this->supervisor_mutex, nullptr);
    pthread_mutex_lock(&this->supervisor_mutex);

    this->supervisor = new ProxySupervisor(socket, &this->supervisor_mutex);
    printf("Connected to proxy.\n");
}

bool Debugger::proxy_connected() const { return this->connected_to_proxy; }

void Debugger::disconnect_proxy() {
    if (this->proxy_connected()) {
        return;
    }
    int *ptr;
    // TODO close file
    pthread_mutex_unlock(&this->supervisor_mutex);
    pthread_join(this->supervisor->getThreadID(), (void **)&ptr);
}

void Debugger::updateCallbackmapping(Module *m,
                                     const debug::CallbackMapping &mapping) {
    CallbackHandler::clear_callbacks();

    for (int i = 0; i < mapping.entries_size(); ++i) {
        const auto &entry = mapping.entries(i);
        for (int j = 0; j < entry.tidx_size(); ++j) {
            CallbackHandler::add_callback(
                Callback(m, entry.topic(), entry.tidx(j)));
        }
    }
}

StackValue *Debugger::readRFCArgs(debug::RFC payload) {
    // TODO
    return nullptr;
}
