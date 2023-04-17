#include "debugger.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#ifndef ARDUINO
#include <nlohmann/json.hpp>
#else
#include "../../lib/json/single_include/nlohmann/json.hpp"
#endif

#include "../Memory/mem.h"
#include "../Utils//util.h"
#include "../Utils/macros.h"

// Debugger

Debugger::Debugger(Channel *duplex) {
    this->channel = duplex;
    this->supervisor_mutex = new std::mutex();
    this->supervisor_mutex->lock();
}

// Public methods

void Debugger::setChannel(Channel *duplex) {
    delete this->channel;
    this->channel = duplex;
}

void Debugger::addDebugMessage(size_t len, const uint8_t *buff) {
    this->parseDebugBuffer(len, buff);
    uint8_t *data{};
    while (!this->parsedInterrupts.empty()) {
        data = this->parsedInterrupts.front();
        this->parsedInterrupts.pop();
        if (*data == interruptRecvCallbackmapping) {
            size_t startIdx = 0;
            size_t endIdx;
            while (buff[startIdx] != '7' || buff[startIdx + 1] != '5' ||
                   buff[startIdx + 2] != '{') {
                startIdx++;
            }
            endIdx = startIdx;
            while (buff[endIdx] != '\n') {
                endIdx++;
            }
            auto *msg = (uint8_t *)acalloc(sizeof(uint8_t), (endIdx - startIdx),
                                           "interrupt buffer");
            memcpy(msg, buff + startIdx, (endIdx - startIdx) * sizeof(uint8_t));
            *msg = *data;
            free(data);
            this->pushMessage(msg);
        } else {
            this->pushMessage(data);
        }
    }
}

void Debugger::pushMessage(uint8_t *msg) {
    std::lock_guard<std::mutex> const lg(messageQueueMutex);
    this->debugMessages.push_back(msg);
    this->freshMessages = !this->debugMessages.empty();
    this->messageQueueConditionVariable.notify_one();
}

void Debugger::parseDebugBuffer(size_t len, const uint8_t *buff) {
    for (size_t i = 0; i < len; i++) {
        bool success = true;
        int r = -1 /*undef*/;

        // TODO replace by real binary
        switch (buff[i]) {
            case '0' ... '9':
                r = buff[i] - '0';
                break;
            case 'A' ... 'F':
                r = buff[i] - 'A' + 10;
                break;
            case 'a' ... 'f':
                r = buff[i] - 'a' + 10;
                break;
            default:
                success = false;
        }

        if (!success) {
            if (this->interruptEven) {
                if (!this->interruptBuffer.empty()) {
                    // done, send to process
                    auto data = (uint8_t *)acalloc(sizeof(uint8_t),
                                                   this->interruptBuffer.size(),
                                                   "interrupt buffer");
                    memcpy(data, this->interruptBuffer.data(),
                           this->interruptBuffer.size() * sizeof(uint8_t));
                    this->parsedInterrupts.push(data);
                    this->interruptBuffer.clear();
                }
            } else {
                this->interruptBuffer.clear();
                this->interruptEven = true;
                dbg_warn("Dropped interrupt: could not process");
            }
        } else {  // good parse
            if (!this->interruptEven) {
                this->interruptLastChar =
                    (this->interruptLastChar << 4u) + (uint8_t)r;
                this->interruptBuffer.push_back(this->interruptLastChar);
            } else {
                this->interruptLastChar = (uint8_t)r;
            }
            this->interruptEven = !this->interruptEven;
        }
    }
}

uint8_t *Debugger::getDebugMessage() {
    std::lock_guard<std::mutex> const lg(messageQueueMutex);
    uint8_t *ret = nullptr;
    if (!this->debugMessages.empty()) {
        ret = this->debugMessages.front();
        this->debugMessages.pop_front();
    }
    this->freshMessages = !this->debugMessages.empty();
    return ret;
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
bool Debugger::checkDebugMessages(Module *m, RunningState *program_state) {
    uint8_t *interruptData = this->getDebugMessage();
    if (interruptData == nullptr) {
        fflush(stdout);
        return false;
    }
    printf("received interrupt %x\n", *interruptData);
    fflush(stdout);

    this->channel->write("Interrupt: %x\n", *interruptData);

    long start = 0, size = 0;
    switch (*interruptData) {
        case interruptRUN:
            this->handleInterruptRUN(m, program_state);
            free(interruptData);
            break;
        case interruptHALT:
            this->channel->write("STOP!\n");
            this->channel->close();
            free(interruptData);
            exit(0);
        case interruptPAUSE:
            *program_state = WARDUINOpause;
            this->channel->write("PAUSE!\n");
            free(interruptData);
            break;
        case interruptSTEP:
            this->channel->write("STEP!\n");
            *program_state = WARDUINOstep;
            this->skipBreakpoint = m->pc_ptr;
            free(interruptData);
            break;
        case interruptBPAdd:  // Breakpoint
        case interruptBPRem:  // Breakpoint remove
            this->handleInterruptBP(interruptData);
            free(interruptData);
            break;
        case interruptDUMP:
            *program_state = WARDUINOpause;
            this->dump(m);
            free(interruptData);
            break;
        case interruptDUMPLocals:
            *program_state = WARDUINOpause;
            this->dumpLocals(m);
            this->channel->write("\n");
            free(interruptData);
            break;
        case interruptDUMPFull:
            *program_state = WARDUINOpause;
            this->dump(m, true);
            free(interruptData);
            break;
        case interruptReset:
            this->reset(m);
            free(interruptData);
            break;
        case interruptUPDATEFun:
            this->channel->write("CHANGE function!\n");
            Debugger::handleChangedFunction(m, interruptData);
            //  do not free(interruptData);
            // we need it to run that code
            // TODO: free double replacements
            break;
        case interruptUPDATELocal:
            this->channel->write("CHANGE local!\n");
            this->handleChangedLocal(m, interruptData);
            free(interruptData);
            break;
        case interruptUPDATEModule:
            this->handleUpdateModule(m, interruptData);
            this->channel->write("CHANGE Module!\n");
            free(interruptData);
            break;
        case interruptUPDATEGlobal:
            this->handleUpdateGlobalValue(m, interruptData + 1);
            free(interruptData);
            break;
        case interruptINVOKE:
            this->handleInvoke(m, interruptData + 1);
            free(interruptData);
            break;
        case interruptWOODDUMP:
            *program_state = WARDUINOpause;
            free(interruptData);
            woodDump(m);
            break;
        case interruptOffset:
            free(interruptData);
            printf("offset\n");
            this->channel->write("{\"offset\":\"%p\"}\n", (void *)m->bytes);
            break;
        case interruptRecvState:
            if (!this->receivingData) {
                *program_state = WARDUINOpause;
                debug("paused program execution\n");
                CallbackHandler::manual_event_resolution = true;
                dbg_info("Manual event resolution is on.");
                this->receivingData = true;
                this->freeState(m, interruptData);
                free(interruptData);
                this->channel->write("ack!\n");
            } else {
                debug("receiving state\n");
                receivingData = !this->saveState(m, interruptData);
                free(interruptData);
                debug("sending %s!\n", receivingData ? "ack" : "done");
                this->channel->write("%s!\n", receivingData ? "ack" : "done");
            }
            break;
        case interruptProxyCall: {
            this->handleProxyCall(m, program_state, interruptData + 1);
            free(interruptData);
        } break;
        case interruptMonitorProxies: {
            printf("receiving functions list to proxy\n");
            this->handleMonitorProxies(m, interruptData + 1);
            free(interruptData);
        } break;
        case interruptProxify: {
            dbg_info("Converting to proxy settings.\n");
            this->proxify();
            free(interruptData);
            break;
        }
        case interruptDUMPAllEvents:
            printf("InterruptDUMPEvents\n");
            size = (long)CallbackHandler::event_count();
        case interruptDUMPEvents:
            // TODO get start and size from message
            this->channel->write("{");
            this->dumpEvents(start, size);
            this->channel->write("}\n");
            free(interruptData);
            break;
        case interruptPOPEvent:
            CallbackHandler::resolve_event(true);
            free(interruptData);
            break;
        case interruptPUSHEvent:
            this->handlePushedEvent(reinterpret_cast<char *>(interruptData));
            free(interruptData);
            break;
        case interruptRecvCallbackmapping:
            Debugger::updateCallbackmapping(
                m, reinterpret_cast<const char *>(interruptData + 2));
            free(interruptData);
            break;
        case interruptDUMPCallbackmapping:
            this->dumpCallbackmapping();
            free(interruptData);
            break;
        default:
            // handle later
            this->channel->write("COULD not parse interrupt data!\n");
            free(interruptData);
            break;
    }
    fflush(stdout);
    return true;
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
            snprintf(buff, 255, R"("type":"F32","value":"%)" PRIx32 "\"",
                     v->value.uint32);
            break;
        case F64:
            snprintf(buff, 255, R"("type":"F64","value":"%)" PRIx64 "\"",
                     v->value.uint64);
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

void Debugger::handleInvoke(Module *m, uint8_t *interruptData) {
    uint32_t fidx = read_LEB_32(&interruptData);

    if (fidx < 0 || fidx >= m->function_count) {
        debug("no function available for fidx %" PRIi32 "\n", fidx);
        return;
    }

    Type func = *m->functions[fidx].type;
    StackValue *args = readWasmArgs(func, interruptData);

    WARDuino *instance = WARDuino::instance();
    RunningState current = instance->program_state;
    instance->program_state = WARDUINOrun;

    WARDuino::instance()->invoke(m, fidx, func.param_count, args);
    instance->program_state = current;
    this->dumpStack(m);
}

void Debugger::handleInterruptRUN(Module *m, RunningState *program_state) {
    this->channel->write("GO!\n");
    if (*program_state == WARDUINOpause && this->isBreakpoint(m->pc_ptr)) {
        this->skipBreakpoint = m->pc_ptr;
    }
    *program_state = WARDUINOrun;
}

void Debugger::handleInterruptBP(const uint8_t *interruptData) {
    // TODO: segfault may happen here!
    uint8_t len = interruptData[1];
    uintptr_t bp = 0x0;
    for (size_t i = 0; i < len; i++) {
        bp <<= sizeof(uint8_t) * 8;
        bp |= interruptData[i + 2];
    }
    auto *bpt = (uint8_t *)bp;
    this->channel->write("BP %p!\n", static_cast<void *>(bpt));

    if (*interruptData == 0x06) {
        this->addBreakpoint(bpt);
    } else {
        this->deleteBreakpoint(bpt);
    }
}

void Debugger::dump(Module *m, bool full) const {
    this->channel->write("{");

    // current PC
    this->channel->write(R"("pc":"%p",)", (void *)m->pc_ptr);

    // start of bytes
    this->channel->write(R"("start":["%p"],)", (void *)m->bytes);

    this->dumpBreakpoints();

    this->dumpFunctions(m);

    this->dumpCallstack(m);

    if (full) {
        this->channel->write(R"(, "locals": )");
        this->dumpLocals(m);
        this->channel->write(", ");
        this->dumpEvents(0, CallbackHandler::event_count());
    }

    this->channel->write("}\n\n");
    //    fflush(stdout);
}

void Debugger::dumpStack(Module *m) const {
    this->channel->write("{\"stack\": [");
    int32_t i = m->sp;
    while (0 <= i) {
        this->printValue(&m->stack[i], i, i < 1);
        i--;
    }
    this->channel->write("]}\n\n");
}

void Debugger::dumpBreakpoints() const {
    this->channel->write("\"breakpoints\":[");
    {
        size_t i = 0;
        for (auto bp : this->breakpoints) {
            this->channel->write(R"("%p"%s)", bp,
                                 (++i < this->breakpoints.size()) ? "," : "");
        }
    }
    this->channel->write("],");
}

void Debugger::dumpFunctions(Module *m) const {
    this->channel->write("\"functions\":[");

    for (size_t i = m->import_count; i < m->function_count; i++) {
        this->channel->write(R"({"fidx":"0x%x","from":"%p","to":"%p"}%s)",
                             m->functions[i].fidx,
                             static_cast<void *>(m->functions[i].start_ptr),
                             static_cast<void *>(m->functions[i].end_ptr),
                             (i < m->function_count - 1) ? "," : "],");
    }
}

/*
 * {"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s
 */
void Debugger::dumpCallstack(Module *m) const {
    this->channel->write("\"callstack\":[");
    for (int i = 0; i <= m->csp; i++) {
        Frame *f = &m->callstack[i];
        uint8_t *callsite = f->ra_ptr - 2;  // callsite of function (if type 0)
        this->channel->write(
            R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"start":"%p","ra":"%p","callsite":"%p"}%s)",
            f->block->block_type, f->block->fidx, f->sp, f->fp,
            f->block->start_ptr, static_cast<void *>(f->ra_ptr),
            static_cast<void *>(callsite), (i < m->csp) ? "," : "]");
    }
}

void Debugger::dumpLocals(Module *m) const {
    //    fflush(stdout);
    int firstFunFramePtr = m->csp;
    while (m->callstack[firstFunFramePtr].block->block_type != 0) {
        firstFunFramePtr--;
        if (firstFunFramePtr < 0) {
            FATAL("Not in a function!");
        }
    }
    Frame *f = &m->callstack[firstFunFramePtr];
    this->channel->write(R"({"count":%u,"locals":[)", f->block->local_count);
    //    fflush(stdout);  // FIXME: this is needed for ESP to properly print
    char _value_str[256];
    for (uint32_t i = 0; i < f->block->local_count; i++) {
        auto v = &m->stack[m->fp + i];
        switch (v->value_type) {
            case I32:
                snprintf(_value_str, 255, R"("type":"i32","value":%)" PRIi32,
                         v->value.uint32);
                break;
            case I64:
                snprintf(_value_str, 255, R"("type":"i64","value":%)" PRIi64,
                         v->value.uint64);
                break;
            case F32:
                snprintf(_value_str, 255, R"("type":"F32","value":%.7f)",
                         v->value.f32);
                break;
            case F64:
                snprintf(_value_str, 255, R"("type":"F64","value":%.7f)",
                         v->value.f64);
                break;
            default:
                snprintf(_value_str, 255,
                         R"("type":"%02x","value":"%)" PRIx64 "\"",
                         v->value_type, v->value.uint64);
        }

        this->channel->write("{%s, \"index\":%u}%s", _value_str,
                             i + f->block->type->param_count,
                             (i + 1 < f->block->local_count) ? "," : "");
    }
    this->channel->write("]}");
    //    fflush(stdout);
}

void Debugger::dumpEvents(long start, long size) const {
    bool previous = CallbackHandler::resolving_event;
    CallbackHandler::resolving_event = true;
    if (size > EVENTS_SIZE) {
        size = EVENTS_SIZE;
    }

    this->channel->write(R"("events": [)");
    long index = start, end = start + size;
    std::for_each(CallbackHandler::event_begin() + start,
                  CallbackHandler::event_begin() + end,
                  [this, &index, &end](const Event &e) {
                      this->channel->write(
                          R"({"topic": "%s", "payload": "%s"})",
                          e.topic.c_str(), e.payload.c_str());
                      if (++index < end) {
                          this->channel->write(", ");
                      }
                  });
    this->channel->write("]");

    CallbackHandler::resolving_event = previous;
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
bool Debugger::handleChangedFunction(Module *m, uint8_t *bytes) {
    // Check if this was a change request
    if (*bytes != interruptUPDATEFun) return false;

    // SKIP the first byte (0x10), type of change
    uint8_t *pos = bytes + 1;

    uint32_t b = read_LEB_32(&pos);  // read id

    Block *function = &m->functions[m->import_count + b];
    uint32_t body_size = read_LEB_32(&pos);
    uint8_t *payload_start = pos;
    uint32_t local_count = read_LEB_32(&pos);
    uint8_t *save_pos;
    uint32_t tidx, lidx, lecount;

    // Local variable handling

    // Get number of locals for alloc
    save_pos = pos;
    function->local_count = 0;
    for (uint32_t l = 0; l < local_count; l++) {
        lecount = read_LEB_32(&pos);
        function->local_count += lecount;
        tidx = read_LEB(&pos, 7);
        (void)tidx;  // TODO: use tidx?
    }

    if (function->local_count > 0) {
        function->local_value_type =
            (uint8_t *)acalloc(function->local_count, sizeof(uint8_t),
                               "function->local_value_type");
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
bool Debugger::handleChangedLocal(Module *m, uint8_t *bytes) const {
    if (*bytes != interruptUPDATELocal) return false;
    uint8_t *pos = bytes + 1;
    this->channel->write("Local updates: %x\n", *pos);
    uint32_t localId = read_LEB_32(&pos);

    this->channel->write("Local %u being changed\n", localId);
    auto v = &m->stack[m->fp + localId];
    switch (v->value_type) {
        case I32:
            v->value.uint32 = read_LEB_signed(&pos, 32);
            break;
        case I64:
            v->value.int64 = read_LEB_signed(&pos, 64);
            break;
        case F32:
            memcpy(&v->value.uint32, pos, 4);
            break;
        case F64:
            memcpy(&v->value.uint64, pos, 8);
            break;
    }
    this->channel->write("Local %u changed to %u\n", localId, v->value.uint32);
    return true;
}

void Debugger::notifyPushedEvent() const {
    this->channel->write("new pushed event");
}

bool Debugger::handlePushedEvent(char *bytes) const {
    if (*bytes != interruptPUSHEvent) return false;
    auto parsed = nlohmann::json::parse(bytes);
    printf("handle pushed event: %s", bytes);
    auto *event = new Event(*parsed.find("topic"), *parsed.find("payload"));
    CallbackHandler::push_event(event);
    this->notifyPushedEvent();
    return true;
}

void Debugger::woodDump(Module *m) {
    debug("asked for doDump\n");
    printf("asked for woodDump\n");
    this->channel->write("DUMP!\n");
    this->channel->write("{");

    // current PC
    this->channel->write(R"("pc":"%p",)", (void *)m->pc_ptr);

    // start of bytes
    this->channel->write(R"("start":["%p"],)", (void *)m->bytes);

    this->channel->write("\"breakpoints\":[");
    size_t i = 0;
    for (auto bp : this->breakpoints) {
        this->channel->write(R"("%p"%s)", bp,
                             (++i < this->breakpoints.size()) ? "," : "");
    }
    this->channel->write("],");

    // stack
    this->channel->write("\"stack\":[");
    for (int j = 0; j <= m->sp; j++) {
        auto v = &m->stack[j];
        printValue(v, j, j == m->sp);
    }
    this->channel->write("],");

    // Callstack
    this->channel->write("\"callstack\":[");
    for (int j = 0; j <= m->csp; j++) {
        Frame *f = &m->callstack[j];
        uint8_t *block_key =
            f->block->block_type == 0 ? nullptr : findOpcode(m, f->block);
        this->channel->write(
            R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"block_key":"%p", "ra":"%p", "idx":%d}%s)",
            f->block->block_type, f->block->fidx, f->sp, f->fp, block_key,
            static_cast<void *>(f->ra_ptr), j, (j < m->csp) ? "," : "");
    }

    // Globals
    this->channel->write("],\"globals\":[");
    for (uint32_t j = 0; j < m->global_count; j++) {
        auto v = m->globals + j;
        printValue(v, j, j == (m->global_count - 1));
    }
    this->channel->write("]");  // closing globals

    this->channel->write(R"(,"table":{"max":%d, "init":%d, "elements":[)",
                         m->table.maximum, m->table.initial);

    for (uint32_t j = 0; j < m->table.size; j++) {
        this->channel->write("%" PRIu32 "%s", m->table.entries[j],
                             (j + 1) == m->table.size ? "" : ",");
    }
    this->channel->write("]}");  // closing table

    // memory
    uint32_t total_elems =
        m->memory.pages * (uint32_t)PAGE_SIZE;  // TODO debug PAGE_SIZE
    this->channel->write(
        R"(,"memory":{"pages":%d,"max":%d,"init":%d,"bytes":[)",
        m->memory.pages, m->memory.maximum, m->memory.initial);
    for (uint32_t j = 0; j < total_elems; j++) {
        this->channel->write("%" PRIu8 "%s", m->memory.bytes[j],
                             (j + 1) == total_elems ? "" : ",");
    }
    this->channel->write("]}");  // closing memory

    this->channel->write(R"(,"br_table":{"size":"0x%x","labels":[)",
                         BR_TABLE_SIZE);
    for (uint32_t j = 0; j < BR_TABLE_SIZE; j++) {
        this->channel->write("%" PRIu32 "%s", m->br_table[j],
                             (j + 1) == BR_TABLE_SIZE ? "" : ",");
    }
    this->channel->write("]}}\n");
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
                m->memory.bytes = (uint8_t *)acalloc(pages * PAGE_SIZE, 1,
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
                debug("receiving pc %p\n", static_cast<void *>(m->pc_ptr));
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
                if (start > limit) {
                    FATAL("incorrect memory offsets\n");
                }
                uint32_t total_bytes = limit - start + 1;
                uint8_t *mem_end =
                    m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
                debug("will copy #%" PRIu32 " bytes from %" PRIu32
                      " to %" PRIu32 " (incl.)\n",
                      total_bytes, start, limit);
                if ((m->memory.bytes + start) + total_bytes > mem_end) {
                    FATAL("memory overflow %p > %p\n",
                          static_cast<void *>(m->bytes + start + total_bytes),
                          static_cast<void *>(mem_end));
                }
                memcpy(m->memory.bytes + start, program_state, total_bytes);
                for (auto i = start; i < (start + total_bytes); i++) {
                    debug("GOT byte idx %" PRIu32 " =%" PRIu8 "\n", i,
                          m->memory.bytes[i]);
                }
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
    WARDuino::instance()->program_state = PROXYhalt;
    this->proxy = new Proxy();  // TODO delete
}

void Debugger::handleProxyCall(Module *m, RunningState *program_state,
                               uint8_t *interruptData) {
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

void Debugger::handleMonitorProxies(Module *m, uint8_t *interruptData) {
    uint32_t amount_funcs = read_B32(&interruptData);
    printf("funcs_total %" PRIu32 "\n", amount_funcs);

    m->warduino->debugger->supervisor->unregisterAllProxiedCalls();
    for (uint32_t i = 0; i < amount_funcs; i++) {
        uint32_t fidx = read_B32(&interruptData);
        printf("registering fid=%" PRIu32 "\n", fidx);
        m->warduino->debugger->supervisor->registerProxiedCall(fidx);
    }

    this->channel->write("done!\n");
}

void Debugger::startProxySupervisor(Channel *socket) {
    this->connected_to_proxy = true;

    this->supervisor = new ProxySupervisor(socket, this->supervisor_mutex);
    printf("Connected to proxy.\n");
}

bool Debugger::proxy_connected() const { return this->connected_to_proxy; }

void Debugger::disconnect_proxy() {
    if (!this->proxy_connected()) {
        return;
    }
    // TODO close file
    this->supervisor_mutex->unlock();
    this->supervisor->thread.join();
}

void Debugger::updateCallbackmapping(Module *m, const char *data) {
    nlohmann::basic_json<> parsed = nlohmann::json::parse(data);
    CallbackHandler::clear_callbacks();
    nlohmann::basic_json<> callbacks = *parsed.find("callbacks");
    for (auto &array : callbacks.items()) {
        auto callback = array.value().begin();
        for (auto &functions : callback.value().items()) {
            CallbackHandler::add_callback(
                Callback(m, callback.key(), functions.value()));
        }
    }
}

// Stop the debugger
void Debugger::stop() {
    if (this->channel != nullptr) {
        this->channel->close();
        this->channel = nullptr;
    }
}

bool Debugger::handleUpdateModule(Module *m, uint8_t *data) {
    uint8_t *wasm_data = data + 1;
    uint32_t wasm_len = read_LEB_32(&wasm_data);
    uint8_t *wasm = (uint8_t *)malloc(sizeof(uint8_t) * wasm_len);
    memcpy(wasm, wasm_data, wasm_len);
    WARDuino *wd = m->warduino;
    wd->update_module(m, wasm, wasm_len);
    return true;
}

bool Debugger::handleUpdateGlobalValue(Module *m, uint8_t *data) {
    this->channel->write("Global updates: %x\n", *data);
    uint32_t index = read_LEB_32(&data);

    if (index >= m->global_count) return false;

    this->channel->write("Global %u being changed\n", index);
    StackValue *v = &m->globals[index];
    bool decodeType = false;
    deserialiseStackValue(data, decodeType, v);
    this->channel->write("Global %u changed to %u\n", index, v->value.uint32);
    return true;
}

bool Debugger::reset(Module *m) {
    auto wasm = (uint8_t *)malloc(sizeof(uint8_t) * m->byte_count);
    memcpy(wasm, m->bytes, m->byte_count);
    m->warduino->update_module(m, wasm, m->byte_count);
    this->channel->write("Reset WARDuino.\n");
    return true;
}

Debugger::~Debugger() {
    this->disconnect_proxy();
    this->stop();
    delete this->supervisor_mutex;
    delete this->supervisor;
}
