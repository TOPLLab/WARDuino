#include "debugger.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#ifndef ARDUINO
#include <nlohmann/json.hpp>
#else
#include "../../lib/json/single_include/nlohmann/json.hpp"
#endif

#include "../Interpreter/proxied.h"
#include "../Memory/mem.h"
#include "../Utils//util.h"
#include "../Utils/macros.h"
#include "../WARDuino/CallbackHandler.h"

// Debugger

Debugger::Debugger(Channel *duplex) {
    this->channel = duplex;
    this->supervisor_mutex = new std::mutex();
    this->supervisor_mutex->lock();
    this->asyncSnapshots = false;
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
    return this->breakpoints.find(loc) != this->breakpoints.end() ||
           this->mark == loc;
}

void Debugger::notifyBreakpoint(Module *m, uint8_t *pc_ptr) {
    this->mark = nullptr;
    uint32_t bp = toVirtualAddress(pc_ptr, m);
    this->channel->write("AT %" PRIu32 "!\n", bp);
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
    debug("received interrupt %x\n", *interruptData);
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
            this->pauseRuntime(m);
            this->channel->write("PAUSE!\n");
            free(interruptData);
            break;
        case interruptSTEP:
            this->handleSTEP(m, program_state);
            free(interruptData);
            break;
        case interruptSTEPOver:
            this->handleSTEPOver(m, program_state);
            free(interruptData);
            break;
        case interruptBPAdd:  // Breakpoint
        case interruptBPRem:  // Breakpoint remove
            this->handleInterruptBP(m, interruptData);
            free(interruptData);
            break;
        case interruptDUMP:
            this->pauseRuntime(m);
            this->dump(m);
            free(interruptData);
            break;
        case interruptDUMPLocals:
            this->pauseRuntime(m);
            this->dumpLocals(m);
            this->channel->write("\n");
            free(interruptData);
            break;
        case interruptDUMPFull:
            this->pauseRuntime(m);
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
        case interruptUPDATEStackValue:
            this->handleUpdateStackValue(m, interruptData + 1);
            free(interruptData);
            break;
        case interruptINVOKE:
            this->handleInvoke(m, interruptData + 1);
            free(interruptData);
            break;
        case interruptSnapshot:
            this->pauseRuntime(m);
            free(interruptData);
            snapshot(m);
            break;
        case interruptEnableSnapshots:
            enableSnapshots(interruptData + 1);
            free(interruptData);
            break;
        case interruptInspect: {
            uint8_t *data = interruptData + 1;
            uint16_t numberBytes = read_B16(&data);
            uint8_t *state = interruptData + 3;
            inspect(m, numberBytes, state);
            free(interruptData);
            break;
        }
        case interruptLoadSnapshot:
            if (!this->receivingData) {
                this->pauseRuntime(m);
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
            debug("receiving functions list to proxy\n");
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
            debug("InterruptDUMPEvents\n");
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
        case interruptStore:
            this->receiveStore(m, interruptData + 1);
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

void Debugger::handleSTEP(Module *m, RunningState *program_state) {
    this->channel->write("STEP!\n");
    *program_state = WARDUINOstep;
    this->skipBreakpoint = m->pc_ptr;
}

void Debugger::handleSTEPOver(Module *m, RunningState *program_state) {
    this->skipBreakpoint = m->pc_ptr;
    uint8_t const opcode = *m->pc_ptr;
    if (opcode == 0x10) {  // step over direct call
        this->mark = m->pc_ptr + 2;
        *program_state = WARDUINOrun;
        // warning: ack will be BP hit
    } else if (opcode == 0x11) {  // step over indirect call
        this->mark = m->pc_ptr + 3;
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
    this->channel->write("BP %" PRIu32 "!\n", virtualAddress);
}

void Debugger::dump(Module *m, bool full) const {
    auto toVA = [m](uint8_t *addr) { return toVirtualAddress(addr, m); };
    this->channel->write("{");

    // current PC
    this->channel->write("\"pc\":%" PRIu32 ",", toVA(m->pc_ptr));

    this->dumpBreakpoints(m);

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

void Debugger::dumpBreakpoints(Module *m) const {
    this->channel->write("\"breakpoints\":[");
    {
        size_t i = 0;
        for (auto bp : this->breakpoints) {
            this->channel->write("%" PRIu32 "%s", toVirtualAddress(bp, m),
                                 (++i < this->breakpoints.size()) ? "," : "");
        }
    }
    this->channel->write("],");
}

void Debugger::dumpFunctions(Module *m) const {
    this->channel->write("\"functions\":[");

    for (size_t i = m->import_count; i < m->function_count; i++) {
        this->channel->write(R"({"fidx":"0x%x",)", m->functions[i].fidx);
        this->channel->write("\"from\":%" PRIu32 ",\"to\":%" PRIu32 "}%s",
                             toVirtualAddress(m->functions[i].start_ptr, m),
                             toVirtualAddress(m->functions[i].end_ptr, m),
                             (i < m->function_count - 1) ? "," : "],");
    }
}

/*
 * {"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s
 */
void Debugger::dumpCallstack(Module *m) const {
    auto toVA = [m](uint8_t *addr) { return toVirtualAddress(addr, m); };
    this->channel->write("\"callstack\":[");
    for (int i = 0; i <= m->csp; i++) {
        Frame *f = &m->callstack[i];
        uint8_t *callsite = nullptr;
        int callsite_retaddr = -1;
        int retaddr = -1;
        // first frame has no retrun address
        if (f->ra_ptr != nullptr) {
            callsite = f->ra_ptr - 2;  // callsite of function (if type 0)
            callsite_retaddr = toVA(callsite);
            retaddr = toVA(f->ra_ptr);
        }
        this->channel->write(R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,)",
                             f->block->block_type, f->block->fidx, f->sp,
                             f->fp);
        this->channel->write("\"start\":%" PRIu32
                             ",\"ra\":%d,\"callsite\":%d}%s",
                             toVA(f->block->start_ptr), retaddr,
                             callsite_retaddr, (i < m->csp) ? "," : "]");
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
    this->channel->write("new pushed event\n");
}

bool Debugger::handlePushedEvent(char *bytes) const {
    if (*bytes != interruptPUSHEvent) return false;
    auto parsed = nlohmann::json::parse(bytes + 1);
    debug("handle pushed event: %s\n", bytes + 1);
    auto *event = new Event(*parsed.find("topic"), *parsed.find("payload"));
    CallbackHandler::push_event(event);
    this->notifyPushedEvent();
    return true;
}

void Debugger::snapshot(Module *m) {
    uint16_t numberBytes = 10;
    uint8_t state[] = {
        pcState,        breakpointsState, callstackState,      globalsState,
        tableState,     memoryState,      branchingTableState, stackState,
        callbacksState, eventsState};
    inspect(m, numberBytes, state);
}

void Debugger::inspect(Module *m, uint16_t sizeStateArray, uint8_t *state) {
    debug("asked for inspect\n");
    uint16_t idx = 0;
    auto toVA = [m](uint8_t *addr) { return toVirtualAddress(addr, m); };
    bool addComma = false;

    this->channel->write("{");

    while (idx < sizeStateArray) {
        switch (state[idx++]) {
            case pcState: {  // PC
                this->channel->write("\"pc\":%" PRIu32 "", toVA(m->pc_ptr));
                addComma = true;

                break;
            }
            case breakpointsState: {
                this->channel->write("%s\"breakpoints\":[",
                                     addComma ? "," : "");
                addComma = true;
                size_t i = 0;
                for (auto bp : this->breakpoints) {
                    this->channel->write(
                        "%" PRIu32 "%s", toVA(bp),
                        (++i < this->breakpoints.size()) ? "," : "");
                }
                this->channel->write("]");
                break;
            }
            case callstackState: {
                this->channel->write("%s\"callstack\":[", addComma ? "," : "");
                addComma = true;
                for (int j = 0; j <= m->csp; j++) {
                    Frame *f = &m->callstack[j];
                    uint8_t bt = f->block->block_type;
                    uint32_t block_key = (bt == 0 || bt == 0xff || bt == 0xfe)
                                             ? 0
                                             : toVA(findOpcode(m, f->block));
                    uint32_t fidx = bt == 0 ? f->block->fidx : 0;
                    int ra = f->ra_ptr == nullptr ? -1 : toVA(f->ra_ptr);
                    this->channel->write(
                        R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"idx":%d,)",
                        bt, fidx, f->sp, f->fp, j);
                    this->channel->write(
                        "\"block_key\":%" PRIu32 ",\"ra\":%d}%s", block_key, ra,
                        (j < m->csp) ? "," : "");
                }
                this->channel->write("]");
                break;
            }
            case stackState: {
                this->channel->write("%s\"stack\":[", addComma ? "," : "");
                addComma = true;
                for (int j = 0; j <= m->sp; j++) {
                    auto v = &m->stack[j];
                    printValue(v, j, j == m->sp);
                }
                this->channel->write("]");
                break;
            }
            case globalsState: {
                this->channel->write("%s\"globals\":[", addComma ? "," : "");
                addComma = true;
                for (uint32_t j = 0; j < m->global_count; j++) {
                    auto v = m->globals + j;
                    printValue(v, j, j == (m->global_count - 1));
                }
                this->channel->write("]");  // closing globals
                break;
            }
            case tableState: {
                this->channel->write(
                    R"(%s"table":{"max":%d, "init":%d, "elements":[)",
                    addComma ? "," : "", m->table.maximum, m->table.initial);
                addComma = true;
                for (uint32_t j = 0; j < m->table.size; j++) {
                    this->channel->write("%" PRIu32 "%s", m->table.entries[j],
                                         (j + 1) == m->table.size ? "" : ",");
                }
                this->channel->write("]}");  // closing table
                break;
            }
            case branchingTableState: {
                this->channel->write(
                    R"(%s"br_table":{"size":"0x%x","labels":[)",
                    addComma ? "," : "", BR_TABLE_SIZE);
                for (uint32_t j = 0; j < BR_TABLE_SIZE; j++) {
                    this->channel->write("%" PRIu32 "%s", m->br_table[j],
                                         (j + 1) == BR_TABLE_SIZE ? "" : ",");
                }
                this->channel->write("]}");
                break;
            }
            case memoryState: {
                uint32_t total_elems = m->memory.pages * (uint32_t)PAGE_SIZE;
                this->channel->write(
                    R"(%s"memory":{"pages":%d,"max":%d,"init":%d,"bytes":[)",
                    addComma ? "," : "", m->memory.pages, m->memory.maximum,
                    m->memory.initial);
                addComma = true;
                for (uint32_t j = 0; j < total_elems; j++) {
                    this->channel->write("%" PRIu8 "%s", m->memory.bytes[j],
                                         (j + 1) == total_elems ? "" : ",");
                }
                this->channel->write("]}");  // closing memory
                break;
            }
            case callbacksState: {
                bool noOuterBraces = false;
                this->channel->write(
                    "%s%s", addComma ? "," : "",
                    CallbackHandler::dump_callbacksV2(noOuterBraces).c_str());
                addComma = true;
                break;
            }
            case eventsState: {
                this->channel->write("%s", addComma ? "," : "");
                this->dumpEvents(0, CallbackHandler::event_count());
                addComma = true;
                break;
            }
            default: {
                debug("dumpExecutionState: Received unknown state request\n");
                break;
            }
        }
    }
    this->channel->write("}\n");
}

void Debugger::enableSnapshots(uint8_t *interruptData) {
    asyncSnapshots = *interruptData;
}

void Debugger::sendAsyncSnapshots(Module *m) {
    if (asyncSnapshots) {
        this->channel->write("SNAPSHOT ");
        snapshot(m);
    }
}

void Debugger::freeState(Module *m, uint8_t *interruptData) {
    debug("freeing the program state\n");
    uint8_t *first_msg = nullptr;
    uint8_t *endfm = nullptr;
    first_msg = interruptData + 1;  // skip interruptLoadSnapshot
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
                    m->table.entries = (uint32_t *)acalloc(
                        size, sizeof(uint32_t), "Module->table.entries");
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
    program_state = interruptData + 1;  // skip interruptLoadSnapshot
    end_state = program_state + read_B32(&program_state);

    debug("saving program_state\n");
    while (program_state < end_state) {
        switch (*program_state++) {
            case pcState: {  // PC
                uint32_t pc = read_B32(&program_state);
                if (!isToPhysicalAddrPossible(pc, m)) {
                    FATAL("cannot set pc on invalid address\n");
                }
                m->pc_ptr = toPhysicalAddress(pc, m);
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
                    m->csp += 1;
                    Frame *f = m->callstack + m->csp;
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
                        m->fp = f->sp + 1;
                    } else if (block_type == 0xff || block_type == 0xfe) {
                        debug("guard block %" PRIu8 "\n", block_type);
                        auto *guard = (Block *)malloc(sizeof(struct Block));
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
                    m->br_table[idx] = el;
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
            case callbacksState: {
                uint32_t numberMappings = read_B32(&program_state);
                for (auto idx = 0; idx < numberMappings; ++idx) {
                    uint32_t callbackKeySize = read_B32(&program_state);
                    char *callbackKey = (char *)malloc(callbackKeySize + 1);
                    memcpy((void *)callbackKey, program_state, callbackKeySize);
                    callbackKey[callbackKeySize] = '\0';
                    program_state += callbackKeySize;
                    uint32_t numberTableIndexes = read_B32(&program_state);
                    for (auto j = 0; j < numberTableIndexes; ++j) {
                        uint32_t tidx = read_B32(&program_state);
                        std::string key{callbackKey};
                        CallbackHandler::add_callback(Callback(m, key, tidx));
                    }
                }
                break;
            }
            case eventsState: {
                uint32_t numberEvents = read_B32(&program_state);
                for (auto idx = 0; idx < numberEvents; ++idx) {
                    // read topic
                    uint32_t topicSize = read_B32(&program_state);
                    char *topic = (char *)malloc(topicSize + 1);
                    memcpy((void *)topic, program_state, topicSize);
                    topic[topicSize] = '\0';
                    program_state += topicSize;

                    // read payload
                    uint32_t payloadSize = read_B32(&program_state);
                    char *payload = (char *)malloc(payloadSize + 1);
                    memcpy((void *)payload, program_state, payloadSize);
                    payload[payloadSize] = '\0';
                    program_state += payloadSize;

                    CallbackHandler::push_event(topic, payload, payloadSize);
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
    delete WARDuino::instance()->interpreter;
    WARDuino::instance()->interpreter = new Proxied();
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

    auto *rfc = new RFC(m, fidx, func->type, args);
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

bool Debugger::isProxy() const { return this->proxy != nullptr; }

bool Debugger::isProxied(uint32_t fidx) const {
    return this->supervisor != nullptr && this->supervisor->isProxied(fidx);
}

void Debugger::handleMonitorProxies(Module *m, uint8_t *interruptData) {
    uint32_t amount_funcs = read_B32(&interruptData);
    debug("funcs_total %" PRIu32 "\n", amount_funcs);

    m->warduino->debugger->supervisor->unregisterAllProxiedCalls();
    for (uint32_t i = 0; i < amount_funcs; i++) {
        uint32_t fidx = read_B32(&interruptData);
        debug("registering fid=%" PRIu32 "\n", fidx);
        m->warduino->debugger->supervisor->registerProxiedCall(fidx);
    }

    this->channel->write("done!\n");
}

void Debugger::startProxySupervisor(Channel *socket) {
    this->connected_to_proxy = true;

    this->supervisor = new ProxySupervisor(socket, this->supervisor_mutex);
    debug("Connected to proxy.\n");
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

void Debugger::receiveStore(Module *m, uint8_t *interruptData) {
    uint8_t *pos = interruptData;
    uint32_t addr = read_LEB_32(&pos);
    auto *sval = (StackValue *)malloc(sizeof(struct StackValue));
    deserialiseStackValue(pos, true, sval);
    m->warduino->interpreter->store(m, sval->value_type, addr, *sval);
    free(sval);
}

// Stop the debugger
void Debugger::stop() {
    if (this->channel != nullptr) {
        this->channel->close();
        this->channel = nullptr;
    }
}

//
void Debugger::pauseRuntime(Module *m) {
    m->warduino->program_state = WARDUINOpause;
    this->mark = nullptr;
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

bool Debugger::handleUpdateStackValue(Module *m, uint8_t *data) {
    uint32_t idx = read_LEB_32(&data);
    if (idx >= STACK_SIZE) {
        return false;
    }
    StackValue *sv = &m->stack[idx];
    bool decodeType = false;
    if (!deserialiseStackValue(data, decodeType, sv)) {
        return false;
    }
    this->channel->write("StackValue %" PRIu32 " changed\n", idx);
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
