#include "interrupt_operations.h"
#include "debug.h"
#include "mem.h"
#include "util.h"


void doDump(Module *m) {
    printf("DUMP!\n");
    printf("{");

    // current PC
    printf(R"("pc":"%p",)", (void *) m->pc_ptr);


    // Functions

    printf("\"fucntions\":[");

    for (size_t i = m->import_count; i < m->function_count; i++) {
        printf(R"({"fidx":"0x%x","from":"%p","to":"%p"}%s)",
               m->functions[i].fidx,
               static_cast<void *>(m->functions[i].start_ptr),
               static_cast<void *>(m->functions[i].end_ptr), (i < m->function_count - 1) ? "," : "],");
    }

    // Callstack

    printf("\"callstack\":[");
    for (int i = 0; i <= m->csp; i++) {
        /*
         * {"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s
         * */
        Frame *f = &m->callstack[i];
        printf(R"({"type":%u,"fidx":"0x%x","sp":%d,"fp":%d,"ra":"%p"}%s)",
               f->block->block_type,
               f->block->fidx,
               f->sp,
               f->fp,
               static_cast<void *>(f->ra_ptr),
               (i < m->csp) ? "," : "]}"
        );
    }

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
    if (*bytes != 0x10) return false;

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
        (void) tidx;  // TODO: use tidx?
    }

    if (function->local_count > 0) {
        function->locals = (uint32_t *) acalloc(
                function->local_count, sizeof(uint32_t), "function->locals");
    }

    // Restore position and read the locals
    pos = save_pos;
    lidx = 0;
    for (uint32_t l = 0; l < local_count; l++) {
        lecount = read_LEB(&pos, 32);
        uint8_t vt = read_LEB(&pos, 7);
        for (uint32_t l = 0; l < lecount; l++) {
            function->locals[lidx++] = vt;
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
 * Validate if there are interrupts and execute them
 *
 * The various kinds of interrups are preceded by an identifier:
 *
 * - `0x01` : Continue running
 * - `0x02` : Halt the execution
 * - `0x03` : Pause execution
 * - `0x04` : Execute one operaion and then pause
 * - `0x05` : Dump information about the program
 * - `0x06` : Add a breakpoint, the adress is specified as a pointer.
 *            The pointer should be specified as: 06[length][pointer]
 *            eg: 06 06 55a5994fa3d6
 * - `0x07` : Remove the breakpoint at the adress specified as a pointer if it
 *            exists (see `0x06`)
 * - `0x10` : Replace the content body of a function by a new function given
 *            as payload (immediately following `0x10`), see #readChange
 */
void check_interrupts(Module *m, RunningState *program_state) {
    uint8_t *interruptData = NULL;
    interruptData = m->warduino->getInterrupt();
    if (interruptData) {
        switch (*interruptData) {
            case 0x01:
                printf("GO!\n");
                *program_state = run;
                free(interruptData);
                break;
            case 0x02:
                printf("STOP!\n");
                free(interruptData);
                exit(0);
                break;
            case 0x03:
                *program_state = pause;
                printf("PAUSE!\n");
                free(interruptData);
                break;
            case 0x04:
                printf("STEP!\n");
                *program_state = step;
                free(interruptData);
                break;
            case 0x05:
                *program_state = pause;
                free(interruptData);
                doDump(m);
                break;
            case 0x06:  // Breakpoint
            case 0x07:  // Breakpoint remove
            {
                // TODO: segfault may happen here!
                uint8_t len = interruptData[1];
                uintptr_t bp = 0x0;
                for (size_t i = 0; i < len; i++) {
                    bp <<= sizeof(uint8_t) * 8;
                    bp |= interruptData[i + 2];
                }
                uint8_t *bpt = (uint8_t *) bp;
                printf("BP %p!\n", static_cast<void *>(bpt));

                if (*interruptData == 0x06) {
                    m->warduino->addBreakpoint(bpt);
                } else {
                    m->warduino->delBreakpoint(bpt);
                }

                free(interruptData);

                break;
            }
            case 0x10:
                printf("CHANGE!\n");
                readChange(m, interruptData);
                //  do not free(interruptData);
                // we need it to run that code
                // TODO: free double replacements
                break;
            default:
                // handle later
                printf("COULD not parse interrupt data!\n");
                free(interruptData);
                break;
        }
    }
}