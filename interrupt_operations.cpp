#include "interrupt_operations.h"
#include <inttypes.h>
#include "string.h"
#include "debug.h"
#include "mem.h"
#include "util.h"

/**
 * Validate if there are interrupts and execute them
 *
 * The various kinds of interrups are preceded by an identifier:
 *
 * - `0x01` : Continue running
 * - `0x02` : Halt the execution
 * - `0x03` : Pause execution
 * - `0x04` : Execute one operaion and then pause
 * - `0x06` : Add a breakpoint, the adress is specified as a pointer.
 *            The pointer should be specified as: 06[length][pointer]
 *            eg: 06 06 55a5994fa3d6
 * - `0x07` : Remove the breakpoint at the adress specified as a pointer if it
 *            exists (see `0x06`)
 * - `0x10` : Dump information about the program
 * - `0x11` :                  show locals
 * - `0x20` : Replace the content body of a function by a new function given
 *            as payload (immediately following `0x10`), see #readChange
 */

enum InteruptTypes {
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

void doDumpLocals(Module *m);

void doDump(Module *m) {
    printf("DUMP!\n");
    printf("{");

    // current PC
    printf(R"("pc":"%p",)", (void *)m->pc_ptr);

    printf("\"breakpoints\":[");

    size_t i = 0;
    for(auto bp : m->warduino->breakpoints){
        printf(R"("%p"%s)",
               bp,
               (++i < m->warduino->breakpoints.size()) ? "," : "");
    }
    printf("],");
    // Functions

    printf("\"functions\":[");

    for (size_t i = m->import_count; i < m->function_count; i++) {
        printf(R"({"fidx":"0x%x","from":"%p","to":"%p"}%s)",
               m->functions[i].fidx,
               static_cast<void *>(m->functions[i].start_ptr),
               static_cast<void *>(m->functions[i].end_ptr),
               (i < m->function_count - 1) ? "," : "],");
    }

    // Callstack

    printf("\"callstack\":[");
    for (int i = 0; i <= m->csp; i++) {
        /*
         * {"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s
         * */
        Frame *f = &m->callstack[i];
        printf(R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s)",
               f->block->block_type, f->block->fidx, f->sp, f->fp,
               static_cast<void *>(f->ra_ptr), (i < m->csp) ? "," : "]}\n");
    }
}

void doDumpLocals(Module *m) {
    fflush(stdout);
    printf("DUMP LOCALS!\n\n");
    fflush(stdout);
    int firstFunFramePtr = m->csp;
    while (m->callstack[firstFunFramePtr].block->block_type != 0) {
        firstFunFramePtr--;
        if (firstFunFramePtr < 0) {
            FATAL("Not in a function!");
        }
    }
    Frame *f = &m->callstack[firstFunFramePtr];
    printf(R"({"count":%u,"locals":[)", 0);
    fflush(stdout);  // FIXME: this is needed for ESP to propery print
    char _value_str[256];
    for (size_t i = 0; i < f->block->local_count; i++) {
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
                snprintf(_value_str, 255, R"("type":"i64","value":%.7f)",
                         v->value.f32);
                break;
            case F64:
                snprintf(_value_str, 255, R"("type":"i64","value":%.7f)",
                         v->value.f64);
                break;
            default:
                snprintf(_value_str, 255,
                         R"("type":"%02x","value":"%)" PRIx64 "\"",
                         v->value_type, v->value.uint64);
        }

        printf("{%s}%s", _value_str, (i+1 < f->block->local_count) ? "," : "");
    }
    printf("]}\n\n");
    fflush(stdout);
}

/**
 * Read the change in bytes array.
 *
 * The array should be of the form
 * [0x10, index, ... new function body 0x0b]
 * Where index is the index without imports
 */
bool readChange(Module *m, uint8_t *bytes) {
    // Check if this was a change request
    if (*bytes != interruptUPDATEFun) return false;

    // SKIP the first byte (0x10), type of change
    uint8_t *pos = bytes + 1;

    uint32_t b = read_LEB(&pos, 32);  // read id

    Block *function = &m->functions[m->import_count + b];
    uint32_t body_size = read_LEB(&pos, 32);
    uint8_t *payload_start = pos;
    uint32_t local_count = read_LEB(&pos, 32);
    uint8_t *save_pos;
    uint32_t tidx, lidx, lecount;

    // Local variable handling

    // Get number of locals for alloc
    save_pos = pos;
    function->local_count = 0;
    for (uint32_t l = 0; l < local_count; l++) {
        lecount = read_LEB(&pos, 32);
        function->local_count += lecount;
        tidx = read_LEB(&pos, 7);
        (void)tidx;  // TODO: use tidx?
    }

    if (function->local_count > 0) {
        function->local_value_type =
            (uint32_t *)acalloc(function->local_count, sizeof(uint32_t),
                                "function->local_value_type");
    }

    // Restore position and read the locals
    pos = save_pos;
    lidx = 0;
    for (uint32_t l = 0; l < local_count; l++) {
        lecount = read_LEB(&pos, 32);
        uint8_t vt = read_LEB(&pos, 7);
        for (uint32_t l = 0; l < lecount; l++) {
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
bool readChangeLocal(Module *m, uint8_t *bytes) {

    if (*bytes != interruptUPDATEFun) return false;
    uint8_t *pos = bytes + 1;
    uint32_t localId = read_LEB(&pos, 32);

    auto v = &m->stack[m->fp + localId];
    switch (v->value_type) {
        case I32:
            v->value.uint32 = read_LEB_signed(&m->pc_ptr, 32);
            break;
        case I64:
            v->value.int64 = read_LEB_signed(&m->pc_ptr, 64);
            break;
        case F32:
            memcpy(&v->value.uint32, pos, 4);
            break;
        case F64:
            memcpy(&v->value.uint64, pos, 8);
            break;
    }
    printf("Local %u changed", localId);
    return true;
}

/**
 * Validate if there are interrupts and execute them
 *
 * The various kinds of interrups are preceded by an identifier:
 *
 * - `0x01` : Continue running
 * - `0x02` : Halt the execution
 * - `0x03` : Pause execution
 * - `0x04` : Execute one operaion and then pause
 * - `0x06` : Add a breakpoint, the adress is specified as a pointer.
 *            The pointer should be specified as: 06[length][pointer]
 *            eg: 06 06 55a5994fa3d6
 * - `0x07` : Remove the breakpoint at the adress specified as a pointer if it
 *            exists (see `0x06`)
 * - `0x10` : Dump information about the program
 * - `0x11` :                  show locals
 * - `0x20` : Replace the content body of a function by a new function given
 *            as payload (immediately following `0x10`), see #readChange
 */
bool check_interrupts(Module *m, RunningState *program_state) {
    uint8_t *interruptData = NULL;
    interruptData = m->warduino->getInterrupt();
    if (interruptData) {
        switch (*interruptData) {
            case interruptRUN:
                printf("GO!\n");
                *program_state = WARDUINOrun;
                free(interruptData);
                break;
            case interruptHALT:
                printf("STOP!\n");
                free(interruptData);
                exit(0);
                break;
            case interruptPAUSE:
                *program_state = WARDUINOpause;
                printf("PAUSE!\n");
                free(interruptData);
                break;
            case interruptSTEP:
                printf("STEP!\n");
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
                uint8_t *bpt = (uint8_t *)bp;
                printf("BP %p!\n", static_cast<void *>(bpt));

                if (*interruptData == 0x06) {
                    m->warduino->addBreakpoint(bpt);
                } else {
                    m->warduino->delBreakpoint(bpt);
                }

                free(interruptData);

                break;
            }

            case interruptDUMP:
                *program_state = WARDUINOpause;
                free(interruptData);
                doDump(m);
                break;
            case interruptDUMPLocals:
                *program_state = WARDUINOpause;
                free(interruptData);
                doDumpLocals(m);
                break;
            case interruptUPDATEFun:
                printf("CHANGE!\n");
                readChange(m, interruptData);
                //  do not free(interruptData);
                // we need it to run that code
                // TODO: free double replacements
                break;
            case interruptUPDATELocal:
                printf("CHANGE!\n");
                readChangeLocal(m, interruptData);
                free(interruptData);
                break;
            default:
                // handle later
                printf("COULD not parse interrupt data!\n");
                free(interruptData);
                break;
        }
        return true;
    }
    return false;
}