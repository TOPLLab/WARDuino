#include "debugger.h"

#include <inttypes.h>

#include <cstring>

#include "../Memory/mem.h"
#include "../Utils//util.h"
#include "../Utils/macros.h"

enum InterruptTypes {
    interruptRUN = 0x01,
    interruptHALT = 0x02,
    interruptPAUSE = 0x03,
    interruptSTEP = 0x04,
    interruptBPAdd = 0x06,
    interruptBPRem = 0x07,
    interruptDUMP = 0x10,
    interruptDUMPLocals = 0x11,
    interruptUPDATEFun = 0x20,
    interruptUPDATELocal = 0x21
};

// Debugger

Debugger::Debugger(int socket) { this->socket = socket; }

// Public methods

void Debugger::addDebugMessage(size_t len, const uint8_t *buff) {
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
            default:
                success = false;
        }

        if (!success) {
            if (this->interruptEven) {
                if (!this->interruptBuffer.empty()) {
                    // done, send to process
                    auto *data = (uint8_t *)acalloc(
                        sizeof(uint8_t), this->interruptBuffer.size(),
                        "interrupt buffer");
                    memcpy(data, this->interruptBuffer.data(),
                           this->interruptBuffer.size() * sizeof(uint8_t));
                    this->debugMessages.push_back(data);
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
    if (!this->debugMessages.empty()) {
        uint8_t *ret = this->debugMessages.front();
        this->debugMessages.pop_front();
        return ret;
    } else {
        return nullptr;
    }
}

void Debugger::addBreakpoint(uint8_t *loc) { this->breakpoints.insert(loc); }

void Debugger::delBreakpoint(uint8_t *loc) { this->breakpoints.erase(loc); }

bool Debugger::isBreakpoint(uint8_t *loc) {
    return this->breakpoints.find(loc) != this->breakpoints.end();
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
 *            eg: 06 06 55a5994fa3d6
 * - `0x07` : Remove the breakpoint at the address specified as a pointer if it
 *            exists (see `0x06`)
 * - `0x10` : Dump information about the program
 * - `0x11` :                  show locals
 * - `0x20` : Replace the content body of a function by a new function given
 *            as payload (immediately following `0x10`), see #readChange
 */
bool Debugger::checkDebugMessages(Module *m, RunningState *program_state) {
    uint8_t *interruptData = nullptr;
    interruptData = this->getDebugMessage();
    if (interruptData) {
        dprintf(this->socket, "Interupt: %x\n", *interruptData);
        switch (*interruptData) {
            case interruptRUN:
                dprintf(this->socket, "GO!\n");
                if (*program_state == WARDUINOpause &&
                    this->isBreakpoint(m->pc_ptr)) {
                    this->skipBreakpoint = m->pc_ptr;
                }
                *program_state = WARDUINOrun;
                free(interruptData);
                break;
            case interruptHALT:
                dprintf(this->socket, "STOP!\n");
                free(interruptData);
                exit(0);
                break;
            case interruptPAUSE:
                *program_state = WARDUINOpause;
                dprintf(this->socket, "PAUSE!\n");
                free(interruptData);
                break;
            case interruptSTEP:
                dprintf(this->socket, "STEP!\n");
                *program_state = WARDUINOstep;
                free(interruptData);
                break;
            case interruptBPAdd:  // Breakpoint
            case interruptBPRem:  // Breakpoint remove
            {
                // TODO: segfault may happen here!
                uint8_t len = interruptData[1];
                uintptr_t bp = 0x0;
                for (size_t i = 0; i < len; i++) {
                    bp <<= sizeof(uint8_t) * 8;
                    bp |= interruptData[i + 2];
                }
                auto *bpt = (uint8_t *)bp;
                dprintf(this->socket, "BP %p!\n", static_cast<void *>(bpt));

                if (*interruptData == 0x06) {
                    this->addBreakpoint(bpt);
                } else {
                    this->delBreakpoint(bpt);
                }

                free(interruptData);

                break;
            }

            case interruptDUMP:
                *program_state = WARDUINOpause;
                free(interruptData);
                this->dumpStack(m);
                break;
            case interruptDUMPLocals:
                *program_state = WARDUINOpause;
                free(interruptData);
                this->dumpLocals(m);
                break;
            case interruptUPDATEFun:
                dprintf(this->socket, "CHANGE local!\n");
                this->readChange(m, interruptData);
                //  do not free(interruptData);
                // we need it to run that code
                // TODO: free double replacements
                break;
            case interruptUPDATELocal:
                dprintf(this->socket, "CHANGE local!\n");
                this->readChangeLocal(m, interruptData);
                free(interruptData);
                break;
            default:
                // handle later
                dprintf(this->socket, "COULD not parse interrupt data!\n");
                free(interruptData);
                break;
        }
        fflush(stdout);
        return true;
    }
    fflush(stdout);
    return false;
}

// Private methods

void Debugger::dumpStack(Module *m) {
    dprintf(this->socket, "DUMP!\n");
    dprintf(this->socket, "{");

    // current PC
    dprintf(this->socket, R"("pc":"%p",)", (void *)m->pc_ptr);

    // start of bytes
    dprintf(this->socket, R"("start":["%p"],)", (void *)m->bytes);

    dprintf(this->socket, "\"breakpoints\":[");

    {
        size_t i = 0;
        for (auto bp : this->breakpoints) {
            dprintf(this->socket, R"("%p"%s)", bp,
                    (++i < this->breakpoints.size()) ? "," : "");
        }
    }
    dprintf(this->socket, "],");
    // Functions

    dprintf(this->socket, "\"functions\":[");

    for (size_t i = m->import_count; i < m->function_count; i++) {
        dprintf(this->socket, R"({"fidx":"0x%x","from":"%p","to":"%p"}%s)",
                m->functions[i].fidx,
                static_cast<void *>(m->functions[i].start_ptr),
                static_cast<void *>(m->functions[i].end_ptr),
                (i < m->function_count - 1) ? "," : "],");
    }

    // Callstack

    dprintf(this->socket, "\"callstack\":[");
    for (int i = 0; i <= m->csp; i++) {
        /*
         * {"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s
         */
        Frame *f = &m->callstack[i];
        dprintf(this->socket,
                R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s)",
                f->block->block_type, f->block->fidx, f->sp, f->fp,
                static_cast<void *>(f->ra_ptr), (i < m->csp) ? "," : "]}\n");
    }
    fflush(stdout);
}

void Debugger::dumpLocals(Module *m) const {
    fflush(stdout);
    dprintf(this->socket, "DUMP LOCALS!\n\n");
    fflush(stdout);
    int firstFunFramePtr = m->csp;
    while (m->callstack[firstFunFramePtr].block->block_type != 0) {
        firstFunFramePtr--;
        if (firstFunFramePtr < 0) {
            FATAL("Not in a function!");
        }
    }
    Frame *f = &m->callstack[firstFunFramePtr];
    dprintf(this->socket, R"({"count":%u,"locals":[)", 0);
    fflush(stdout);  // FIXME: this is needed for ESP to propery print
    char _value_str[256];
    for (size_t i = 0; i < f->block->local_count; i++) {
        auto v = &m->stack[m->fp + i];
        switch (v->value_type) {
            case I32:
                snprintf(_value_str, 255,
                          R"("type":"i32","value":%)" PRIi32, v->value.uint32);
                break;
            case I64:
                snprintf(_value_str, 255,
                          R"("type":"i64","value":%)" PRIi64, v->value.uint64);
                break;
            case F32:
                snprintf(_value_str, 255,
                          R"("type":"i64","value":%.7f)", v->value.f32);
                break;
            case F64:
                snprintf(_value_str, 255,
                          R"("type":"i64","value":%.7f)", v->value.f64);
                break;
            default:
                snprintf(_value_str, 255,
                          R"("type":"%02x","value":"%)" PRIx64 "\"",
                          v->value_type, v->value.uint64);
        }

        dprintf(this->socket, "{%s}%s", _value_str,
                (i + 1 < f->block->local_count) ? "," : "");
    }
    dprintf(this->socket, "]}\n\n");
    fflush(stdout);
}

/**
 * Read the change in bytes array.
 *
 * The array should be of the form
 * [0x10, index, ... new function body 0x0b]
 * Where index is the index without imports
 */
bool Debugger::readChange(Module *m, uint8_t *bytes) {
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
bool Debugger::readChangeLocal(Module *m, uint8_t *bytes) const {
    if (*bytes != interruptUPDATELocal) return false;
    uint8_t *pos = bytes + 1;
    dprintf(this->socket, "Local updates: %x\n", *pos);
    uint32_t localId = read_LEB_32(&pos);

    dprintf(this->socket, "Local %u being cahnged\n", localId);
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
    dprintf(this->socket, "Local %u changed to %u\n", localId, v->value.uint32);
    return true;
}