#include "WARDuino.h"
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "primitives.h"

extern "C" {
#include "mem.h"
#include "util.h"
}

// UTIL
bool resolvesym(char *filename, char *symbol, Primitive *val, char **err) {
    if (NULL != filename && !strcmp(filename, "esp8266")) {
        return resolve_primitive(symbol, val);
    } else {
        *err = (char *)"Imports are only supported from the module esp8266";
        return false;
    }
}

// Size of memory load.
// This starts with the first memory load operator at opcode 0x28
uint32_t LOAD_SIZE[] = {4, 8, 4, 8, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4,  // loads
                        4, 8, 4, 8, 1, 2, 1, 2, 4};                // stores

// global exception message
// char  exception[4096];
char exception[512];

// Static definition of block_types
uint32_t block_type_results[4][1] = {{I32}, {I64}, {F32}, {F64}};

Type block_types[5];

void initTypes() {
    block_types[0].form = BLOCK;
    block_types[0].result_count = 0;
    block_types[1].form = BLOCK;
    block_types[1].result_count = 1;
    block_types[1].results = block_type_results[0];
    block_types[2].form = BLOCK;
    block_types[2].result_count = 1;
    block_types[2].results = block_type_results[1];
    block_types[3].form = BLOCK;
    block_types[3].result_count = 1;
    block_types[3].results = block_type_results[2];
    block_types[4].form = BLOCK;
    block_types[4].result_count = 1;
    block_types[4].results = block_type_results[3];
}

Type *get_block_type(uint8_t value_type) {
    switch (value_type) {
        case 0x40:
            return &block_types[0];
        case I32:
            return &block_types[1];
        case I64:
            return &block_types[2];
        case F32:
            return &block_types[3];
        case F64:
            return &block_types[4];
        default:
            FATAL("invalid block_type value_type: %d\n", value_type);
            return NULL;
    }
}

// TODO: calculate this while parsing types
uint64_t get_type_mask(Type *type) {
    uint64_t mask = 0x80;

    if (type->result_count == 1) {
        mask |= 0x80 - type->results[0];
    }
    mask = mask << 4;
    for (uint32_t p = 0; p < type->param_count; p++) {
        mask = ((uint64_t)mask) << 4;
        mask |= 0x80 - type->params[p];
    }
    return mask;
}

char _value_str[256];

char *value_repr(StackValue *v) {
    switch (v->value_type) {
        case I32:
            snprintf(_value_str, 255, "0x%" PRIx32 ":i32", v->value.uint32);
            break;
        case I64:
            snprintf(_value_str, 255, "0x%" PRIx64 ":i64", v->value.uint64);
            break;
        case F32:
            snprintf(_value_str, 255, "%.7g:f32", v->value.f32);
            break;
        case F64:
            snprintf(_value_str, 255, "%.7g:f64", v->value.f64);
            break;
    }
    return _value_str;
}

char _block_str[1024];
char *block_repr(Block *b) {
    if (b->block_type == 0) {
        snprintf(_block_str, 1023, "fn0x%x<%d/%d->%d>", b->fidx,
                 b->type->param_count, b->local_count, b->type->result_count);
    } else {
        snprintf(_block_str, 1023, "%s<0/0->%d>",
                 b->block_type == 0x01
                     ? "init"
                     : b->block_type == 0x02
                           ? "block"
                           : b->block_type == 0x03 ? "loop" : "if",
                 b->type->result_count);
    }
    return _block_str;
}

void dump_stacks(Module *m) {
    warn("      * stack:     [");
    for (int i = 0; i <= m->sp; i++) {
        if (i == m->fp) {
            warn("* ");
        }
        warn("%s", value_repr(&m->stack[i]));
        if (i != m->sp) {
            warn(" ");
        }
    }
    warn("]\n");
    warn("      * callstack: [");
    for (int i = 0; i <= m->csp; i++) {
        Frame *f = &m->callstack[i];
        (void)f;
        warn("%s(sp:%d/fp:%d/ra:0x%x)", block_repr(f->block), f->sp, f->fp,
             f->ra);
        if (i != m->csp) {
            warn(" ");
        }
    }
    warn("]\n");
}

void parse_table_type(Module *m, uint32_t *pos) {
    m->table.elem_type = read_LEB(m->bytes, pos, 7);
    ASSERT(m->table.elem_type == ANYFUNC, "Table elem_type 0x%x unsupported",
           m->table.elem_type);

    uint32_t flags = read_LEB(m->bytes, pos, 32);
    uint32_t tsize = read_LEB(m->bytes, pos, 32);  // Initial size
    m->table.initial = tsize;
    m->table.size = tsize;
    // Limit maximum to 64K
    if (flags & 0x1) {
        tsize = read_LEB(m->bytes, pos, 32);  // Max size
        m->table.maximum = 0x10000 < tsize ? 0x10000 : tsize;
    } else {
        m->table.maximum = 0x10000;
    }
    debug("  table size: %d\n", tsize);
}

void parse_memory_type(Module *m, uint32_t *pos) {
    uint32_t flags = read_LEB(m->bytes, pos, 32);
    uint32_t pages = read_LEB(m->bytes, pos, 32);  // Initial size
    m->memory.initial = pages;
    m->memory.pages = pages;
    // Limit the maximum to 2GB
    if (flags & 0x1) {
        pages = read_LEB(m->bytes, pos, 32);  // Max size
        m->memory.maximum = (uint32_t)fmin(0x8000, pages);
    } else {
        m->memory.maximum = 0x8000;
    }
}

void skip_immediates(uint8_t *bytes, uint32_t *pos) {
    uint32_t count, opcode = bytes[*pos];
    *pos = *pos + 1;
    switch (opcode) {
        // varuint1
        case 0x3f ... 0x40:  // current_memory, grow_memory
            read_LEB(bytes, pos, 1);
            break;
            // varuint32, varint32
        case 0x0c ... 0x0d:  // br, br_if
        case 0x10:           // call
        case 0x20 ... 0x24:  // get/set_local, tee_local, get/set_global
        case 0x41:           // i32.const
            read_LEB(bytes, pos, 32);
            break;
            // varuint32 + varuint1
        case 0x11:  // call_indirect
            read_LEB(bytes, pos, 1);
            read_LEB(bytes, pos, 32);
            break;
            // varint64
        case 0x42:  // i64.const
            read_LEB(bytes, pos, 64);
            break;
            // uint32
        case 0x43:  // f32.const
            *pos += 4;
            break;
            // uint64
        case 0x44:  // f64.const
            *pos += 8;
            break;
            // block_type
        case 0x02 ... 0x04:  // block, loop, if
            read_LEB(bytes, pos, 7);
            break;
            // memory_immediate
        case 0x28 ... 0x3e:  // *.load*, *.store*
            read_LEB(bytes, pos, 32);
            read_LEB(bytes, pos, 32);
            break;
            // br_table
        case 0x0e:                             // br_table
            count = read_LEB(bytes, pos, 32);  // target count
            for (uint32_t i = 0; i < count; i++) {
                read_LEB(bytes, pos, 32);
            }
            read_LEB(bytes, pos, 32);  // default target
            break;
        default:  // no immediates
            break;
    }
}

void find_blocks(Module *m) {
    Block *function;
    Block *block;
    Block *blockstack[BLOCKSTACK_SIZE];
    int top = -1;
    uint8_t opcode = 0x00;
    info("  find_blocks: function_count: %d\n", m->function_count);
    for (uint32_t f = m->import_count; f < m->function_count; f++) {
        function = &m->functions[f];
        debug("    fidx: 0x%x, start: 0x%x, end: 0x%x\n", f,
              function->start_addr, function->end_addr);
        uint32_t pos = function->start_addr;
        while (pos <= function->end_addr) {
            opcode = m->bytes[pos];
            switch (opcode) {
                case 0x02:  // block
                case 0x03:  // loop
                case 0x04:  // if
                    block = (Block *)acalloc(1, sizeof(Block), "Block");
                    block->block_type = opcode;
                    block->type = get_block_type(m->bytes[pos + 1]);
                    block->start_addr = pos;
                    blockstack[++top] = block;
                    m->block_lookup[pos] = block;
                    break;
                case 0x05:  // else
                    ASSERT(blockstack[top]->block_type == 0x04,
                           "else not matched with if")
                    blockstack[top]->else_addr = pos + 1;
                    break;
                case 0x0b:  // end
                    if (pos == function->end_addr) {
                        break;
                    }
                    ASSERT(top >= 0, "blockstack underflow");
                    block = blockstack[top--];
                    block->end_addr = pos;
                    if (block->block_type == 0x03) {
                        // loop: label after start
                        block->br_addr = block->start_addr + 2;
                    } else {
                        // block, if: label at end
                        block->br_addr = pos;
                    }
                    debug(
                        "      block start: 0x%x, end: 0x%x,"
                        " br_addr: 0x%x, else_addr: 0x%x\n",
                        block->start_addr, block->end_addr, block->br_addr,
                        block->else_addr);
                    break;
            }
            skip_immediates(m->bytes, &pos);
        }

        ASSERT(top == -1, "Function ended in middle of block\n")
        ASSERT(opcode == 0x0b, "Function block did not end with 0xb\n")
    }
}

//
// Stack machine (byte code related functions)
//
void push_block(Module *m, Block *block, int sp) {
    m->csp += 1;
    m->callstack[m->csp].block = block;
    m->callstack[m->csp].sp = sp;
    m->callstack[m->csp].fp = m->fp;
    m->callstack[m->csp].ra = m->pc;
}

Block *pop_block(Module *m) {
    Frame *frame = &m->callstack[m->csp--];
    Type *t = frame->block->type;

    // TODO: validate return value if there is one

    m->fp = frame->fp;  // Restore frame pointer

    // Validate the return value
    if (t->result_count == 1) {
        if (m->stack[m->sp].value_type != t->results[0]) {
            sprintf(exception, "call type mismatch");
            return NULL;
        }
    }

    // Restore stack pointer
    if (t->result_count == 1) {
        // Save top value as result
        if (frame->sp < m->sp) {
            m->stack[frame->sp + 1] = m->stack[m->sp];
            m->sp = frame->sp + 1;
        }
    } else {
        if (frame->sp < m->sp) {
            m->sp = frame->sp;
        }
    }

    if (frame->block->block_type == 0x00) {
        // Function, set pc to return address
        m->pc = frame->ra;
    }

    return frame->block;
}

// Setup a function
// Push params and locals on the stack and save a call frame on the call stack
// Sets new pc value for the start of the function
void setup_call(Module *m, uint32_t fidx) {
    Block *func = &m->functions[fidx];
    Type *type = func->type;

    // Push current frame on the call stack
    push_block(m, func, m->sp - type->param_count);

    if (TRACE) {
        warn("  >> fn0x%x(%d) %s(", fidx, fidx,
             func->export_name ? func->export_name : "");
        for (int p = type->param_count - 1; p >= 0; p--) {
            warn("%s%s", value_repr(&m->stack[m->sp - p]), p ? " " : "");
        }
        warn("), %d locals, %d results\n", func->local_count,
             type->result_count);
    }

    // Push locals (dropping extras)
    m->fp = m->sp - type->param_count + 1;
    // TODO: validate arguments vs formal params

    // Push function locals
    for (uint32_t lidx = 0; lidx < func->local_count; lidx++) {
        m->sp += 1;
        m->stack[m->sp].value_type = func->locals[lidx];
        m->stack[m->sp].value.uint64 = 0;  // Initialize whole union to 0
    }

    // Set program counter to start of function
    m->pc = func->start_addr;
    return;
}

/*
   WebAssembly Instructions
   */

/*
   Control Instructions :=
   | nop
   | unreachable
   | block resulttype instr* end
   | loop resulttype instr* end
   | if resulttype instr* else instr* end
   | br labelidx
   | br_if labelidx
   | br_table vec(labelidx) labelidx
   | return
   | call funcidx
   | call_indirect typeidx
   */

/*
Instruction: call funcidx

Webassembly Description:

1. Let 𝐹 be the current frame.
2. Assert: due to validation, 𝐹.module.funcaddrs[𝑥] exists.
3. Let 𝑎 be the function address 𝐹.module.funcaddrs[𝑥].
4. Invoke the function instance at address 𝑎.

Formal specification:

𝐹.module.funcaddrs[𝑥] = 𝑎
-----------------------------------
𝐹; (call 𝑥) -> 𝐹; (invoke 𝑎)
*/

/**
 * 0x02
 */
bool i_instr_block(Module *m, uint32_t *cur_pc) {
    read_LEB(m->bytes, &m->pc, 32);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    push_block(m, m->block_lookup[*cur_pc], m->sp);
    return true;
}

/**
 * 0x03
 */
bool i_instr_loop(Module *m, uint32_t *cur_pc) {
    read_LEB(m->bytes, &m->pc, 32);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    push_block(m, m->block_lookup[*cur_pc], m->sp);
    return true;
}

/**
 * 0x04 if
 */
bool i_instr_if(Module *m, uint32_t *cur_pc) {
    read_LEB(m->bytes, &m->pc, 32);  // ignore block type
    Block *block = m->block_lookup[*cur_pc];
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    push_block(m, block, m->sp);

    uint32_t cond = m->stack[m->sp--].value.uint32;
    if (cond == 0) {  // if false (I32)
        // branch to else block or after end of if
        if (block->else_addr == 0) {
            // no else block, pop if block and skip end
            m->csp -= 1;
            m->pc = block->br_addr + 1;
        } else {
            m->pc = block->else_addr;
        }
    }
    // if true, keep going
    if (TRACE) {
        debug("      - cond: 0x%x jump to 0x%x, block: %s\n", cond, m->pc,
              block_repr(block));
    }
    return true;
}

/**
 * 0x05 else
 */
bool i_instr_else(Module *m, uint32_t *cur_pc) {
    Block *block = m->callstack[m->csp].block;
    m->pc = block->br_addr;
    if (TRACE) {
        debug("      - of %s jump to 0x%x\n", block_repr(block), m->pc);
    }
    return true;
}

/**
 * 0x0b end
 */
bool i_instr_end(Module *m, uint32_t *cur_pc) {
    Block *block = pop_block(m);
    if (block == NULL) {
        return false;  // an exception (set by pop_block)
    }
    if (TRACE) {
        debug("      - of %s\n", block_repr(block));
    }
    if (block->block_type == 0x00) {  // Function
        if (TRACE) {
            warn("  << fn0x%x(%d) %s = %s\n", block->fidx, block->fidx,
                 block->export_name ? block->export_name : "",
                 block->type->result_count > 0 ? value_repr(&m->stack[m->sp])
                                               : "_");
        }
        if (m->csp == -1) {
            // Return to top-level
            return true;
        } else {
            // Keep going at return address
        }
    } else if (block->block_type == 0x01) {  // init_expr
        return true;
    } else {  // Block
              // End of block/loop/if, keep going
    }
    return true;
}

/**
 * 0x0c br
 */
bool i_instr_br(Module *m, uint32_t *cur_pc) {
    uint32_t depth = read_LEB(m->bytes, &m->pc, 32);
    m->csp -= depth;
    // set to end for pop_block
    m->pc = m->callstack[m->csp].block->br_addr;
    if (TRACE) {
        debug("      - to: 0x%p\n", &m->pc);
    }
    return true;
}

/**
 * 0x0d br_if
 */
bool i_instr_br_if(Module *m, uint32_t *cur_pc) {
    uint32_t depth = read_LEB(m->bytes, &m->pc, 32);

    uint32_t cond = m->stack[m->sp--].value.uint32;
    if (cond) {  // if true
        m->csp -= depth;
        // set to end for pop_block
        m->pc = m->callstack[m->csp].block->br_addr;
    }
    if (TRACE) {
        debug("      - depth: 0x%x, cond: 0x%x, to: 0x%x\n", depth, cond,
              m->pc);
    }

    return true;
}

/**
 * 0x0e br_table
 */
bool i_instr_br_table(Module *m, uint32_t *cur_pc) {
    uint32_t count = read_LEB(m->bytes, &m->pc, 32);
    if (count > BR_TABLE_SIZE) {
        // TODO: check this prior to runtime
        sprintf(exception, "br_table size %d exceeds max %d\n", count,
                BR_TABLE_SIZE);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        m->br_table[i] = read_LEB(m->bytes, &m->pc, 32);
    }
    uint32_t depth = read_LEB(m->bytes, &m->pc, 32);

    int32_t didx = m->stack[m->sp--].value.int32;
    if (didx >= 0 && didx < (int32_t)count) {
        depth = m->br_table[didx];
    }

    m->csp -= depth;
    // set to end for pop_block
    m->pc = m->callstack[m->csp].block->br_addr;
    if (TRACE) {
        debug("      - count: %d, didx: %d, to: 0x%x\n", count, didx, m->pc);
    }
    return true;
}

/**
 * 0x0f return
 */
bool i_instr_return(Module *m, uint32_t *cur_pc) {
    while (m->csp >= 0 && m->callstack[m->csp].block->block_type != 0x00) {
        m->csp--;
    }
    // Set the program count to the end of the function
    // The actual pop_block and return is handled by the end opcode.
    m->pc = m->callstack[0].block->end_addr;
    if (TRACE) {
        debug("      - to: 0x%x\n", m->pc);
    }
    return true;
}

/**
 * 0x10 call
 */
bool i_instr_call(Module *m, uint32_t *cur_pc) {
    uint32_t fidx = read_LEB(m->bytes, &m->pc, 32);
    if (fidx < m->import_count) {
        ((Primitive)m->functions[fidx].func_ptr)(m);
    } else {
        if (m->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            return false;
        }
        setup_call(m, fidx);  // regular function call
        if (TRACE) {
            debug("      - calling function fidx: %d at: 0x%x\n", fidx, m->pc);
        }
    }
    return true;
}

/**
 * 0x11 call_indirect
 */
bool i_instr_call_indirect(Module *m, uint32_t *cur_pc) {
    uint32_t tidx = read_LEB(m->bytes, &m->pc, 32);  // TODO: use tidx?
    (void)tidx;
    read_LEB(m->bytes, &m->pc, 1);  // reserved immediate
    uint32_t val = m->stack[m->sp--].value.uint32;
    if (m->options.mangle_table_index) {
        // val is the table address + the index (not sized for the
        // pointer size) so get the actual (sized) index
        if (TRACE) {
            debug("      - entries: %p, original val: 0x%x, new val: 0x%x\n",
                  m->table.entries, val,
                  (uint32_t)((uint64_t)m->table.entries) - val);
        }
        // val = val - (uint32_t)((uint64_t)m->table.entries & 0xFFFFFFFF);
        val = val - (uint32_t)((uint64_t)m->table.entries);
    }
    if (val >= m->table.maximum) {
        sprintf(exception, "undefined element 0x%x (max: 0x%x) in table", val,
                m->table.maximum);
        return false;
    }

    uint32_t fidx = m->table.entries[val];
    if (TRACE) {
        debug("       - call_indirect tidx: %d, val: 0x%x, fidx: 0x%x\n", tidx,
              val, fidx);
    }

    if (fidx < m->import_count) {
        // THUNK thunk_out(m, fidx);    // import/thunk call
    } else {
        Block *func = &m->functions[fidx];
        Type *ftype = func->type;

        if (m->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            return false;
        }
        if (ftype->mask != m->types[tidx].mask) {
            sprintf(exception,
                    "indirect call type mismatch (call type and function type "
                    "differ)");
            return false;
        }

        setup_call(m, fidx);  // regular function call

        // Validate signatures match
        if ((int)(ftype->param_count + func->local_count) !=
            m->sp - m->fp + 1) {
            sprintf(exception,
                    "indirect call type mismatch (param counts differ)");
            return false;
        }
        for (uint32_t f = 0; f < ftype->param_count; f++) {
            if (ftype->params[f] != m->stack[m->fp + f].value_type) {
                sprintf(exception,
                        "indirect call type mismatch (param types differ)");
                return false;
            }
        }

        if (TRACE) {
            debug(
                "      - tidx: %d, table idx: %d, "
                "calling function fidx: %d at: 0x%x\n",
                tidx, val, fidx, m->pc);
        }
    }
    return true;
}

/**
 * 0x1a drop
 * remvove a value from the stack
 */
bool i_instr_drop(Module *m, uint32_t *cur_pc) {
    m->sp--;
    return true;
}

/**
 * 0x1b select
 *
 * pop the c from the stack,
 * pop val_2 form the stack
 * pop val_1 form the stack
 * if c : push val_1 to the stack
 * else : push val_2 to the stack
 */
bool i_instr_select(Module *m, uint32_t *cur_pc) {
    uint32_t cond = m->stack[m->sp--].value.uint32;
    m->sp--;
    if (!cond) {  // use a instead of b
        m->stack[m->sp] = m->stack[m->sp + 1];
    }
    return true;
}

/**
 * 0x20 get_local
 * move the i-th local to the top of the stack
 */
bool i_instr_get_local(Module *m, uint32_t *cur_pc) {
    int32_t arg = read_LEB(m->bytes, &m->pc, 32);
    if (TRACE) {
        debug("      - arg: 0x%x, got %s\n", arg,
              value_repr(&m->stack[m->fp + arg]));
    }
    m->stack[++m->sp] = m->stack[m->fp + arg];
    return true;
}

/**
 * 0x21 set_local
 */
bool i_instr_set_local(Module *m, uint32_t *cur_pc) {
    int32_t arg = read_LEB(m->bytes, &m->pc, 32);
    m->stack[m->fp + arg] = m->stack[m->sp--];
    if (TRACE) {
        debug("      - arg: 0x%x, to %s\n", arg, value_repr(&m->stack[m->sp]));
    }
    return true;
}

/**
 * 0x0d tee_local
 */
bool i_instr_tee_local(Module *m, uint32_t *cur_pc) {
    int32_t arg = read_LEB(m->bytes, &m->pc, 32);
    m->stack[m->fp + arg] = m->stack[m->sp];
    if (TRACE) {
        debug("      - arg: 0x%x, to %s\n", arg, value_repr(&m->stack[m->sp]));
    }
    return true;
}

/**
 * 0x24 set_global
 */
bool i_instr_set_global(Module *m, uint32_t *cur_pc) {
    uint32_t arg = read_LEB(m->bytes, &m->pc, 32);
    if (TRACE) {
        debug("      - arg: 0x%x, got %s\n", arg, value_repr(&m->globals[arg]));
    }
    m->stack[++m->sp] = m->globals[arg];
    return true;
}

/**
 * 0x3f current_memory
 */
bool i_instr_current_memory(Module *m, uint32_t *cur_pc) {
    read_LEB(m->bytes, &m->pc, 32);  // ignore reserved
    m->stack[++m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = m->memory.pages;
    return true;
}

/**
 * 0x40 grow_memory
 */
bool i_instr_grow_memory(Module *m, uint32_t *cur_pc) {
    read_LEB(m->bytes, &m->pc, 32);  // ignore reserved
    uint32_t prev_pages = m->memory.pages;
    uint32_t delta = m->stack[m->sp].value.uint32;
    m->stack[m->sp].value.uint32 = prev_pages;
    if (delta == 0) {
        return true;  // No change
    } else if (delta + prev_pages > m->memory.maximum) {
        m->stack[m->sp].value.uint32 = -1;
        return true;
    }
    m->memory.pages += delta;
    m->memory.bytes = (uint8_t *)arecalloc(
        m->memory.bytes, prev_pages * PAGE_SIZE, m->memory.pages * PAGE_SIZE,
        sizeof(uint32_t), "Module->memory.bytes");
    return true;
}

/**
 * 0x0d XXX
 */
bool i_instr_mem_load(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    bool overflow = false;
    uint8_t *maddr, *mem_end;
    uint32_t flags = read_LEB(m->bytes, &m->pc, 32);
    uint32_t offset = read_LEB(m->bytes, &m->pc, 32);
    uint32_t addr = m->stack[m->sp--].value.uint32;
    if (flags != 2 && TRACE) {
        info(
            "      - unaligned load - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x\n",
            flags, offset, addr);
    }
    if (offset + addr < addr) {
        overflow = true;
    }
    maddr = m->memory.bytes + offset + addr;
    if (maddr < m->memory.bytes) {
        overflow = true;
    }
    mem_end = m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + LOAD_SIZE[opcode - 0x28] > mem_end) {
        overflow = true;
    }
    info("      - addr: 0x%x, offset: 0x%x, maddr: %p, mem_end: %p\n", addr,
         offset, maddr, mem_end);
    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            warn("memory start: %p, memory end: %p, maddr: %p\n",
                 m->memory.bytes, mem_end, maddr);
            sprintf(exception, "out of bounds memory access");
            return false;
        }
    }
    m->stack[++m->sp].value.uint64 = 0;  // initialize to 0
    switch (opcode) {
        case 0x28:
            memcpy(&m->stack[m->sp].value, maddr, 4);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load
        case 0x29:
            memcpy(&m->stack[m->sp].value, maddr, 8);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load
        case 0x2a:
            memcpy(&m->stack[m->sp].value, maddr, 4);
            m->stack[m->sp].value_type = F32;
            break;  // f32.load
        case 0x2b:
            memcpy(&m->stack[m->sp].value, maddr, 8);
            m->stack[m->sp].value_type = F64;
            break;  // f64.load
        case 0x2c:
            memcpy(&m->stack[m->sp].value, maddr, 1);
            sext_8_32(&m->stack[m->sp].value.uint32);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load8_s
        case 0x2d:
            memcpy(&m->stack[m->sp].value, maddr, 1);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load8_u
        case 0x2e:
            memcpy(&m->stack[m->sp].value, maddr, 2);
            sext_16_32(&m->stack[m->sp].value.uint32);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load16_s
        case 0x2f:
            memcpy(&m->stack[m->sp].value, maddr, 2);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load16_u
        case 0x30:
            memcpy(&m->stack[m->sp].value, maddr, 1);
            sext_8_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load8_s
        case 0x31:
            memcpy(&m->stack[m->sp].value, maddr, 1);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load8_u
        case 0x32:
            memcpy(&m->stack[m->sp].value, maddr, 2);
            sext_16_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load16_s
        case 0x33:
            memcpy(&m->stack[m->sp].value, maddr, 2);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load16_u
        case 0x34:
            memcpy(&m->stack[m->sp].value, maddr, 4);
            sext_32_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load32_s
        case 0x35:
            memcpy(&m->stack[m->sp].value, maddr, 4);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load32_u
    }
    return true;
}

bool i_instr_mem_store(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    uint8_t *maddr, *mem_end;
    uint32_t flags = read_LEB(m->bytes, &m->pc, 32);
    uint32_t offset = read_LEB(m->bytes, &m->pc, 32);
    StackValue *sval = &m->stack[m->sp--];
    uint32_t addr = m->stack[m->sp--].value.uint32;
    bool overflow = false;

    if (flags != 2 && TRACE) {
        info(
            "      - unaligned store - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x, val: %s\n",
            flags, offset, addr, value_repr(sval));
    }
    if (offset + addr < addr) {
        overflow = true;
    }
    maddr = m->memory.bytes + offset + addr;
    if (maddr < m->memory.bytes) {
        overflow = true;
    }
    mem_end = m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + LOAD_SIZE[opcode - 0x28] > mem_end) {
        overflow = true;
    }
    info(
        "      - addr: 0x%x, offset: 0x%x, maddr: %p, mem_end: %p, value: "
        "%s\n",
        addr, offset, maddr, mem_end, value_repr(sval));
    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            warn("memory start: %p, memory end: %p, maddr: %p\n",
                 m->memory.bytes, mem_end, maddr);
            sprintf(exception, "out of bounds memory access");
            return false;
        }
    }
    switch (opcode) {
        case 0x36:
            memcpy(maddr, &sval->value.uint32, 4);
            break;  // i32.store
        case 0x37:
            memcpy(maddr, &sval->value.uint64, 8);
            break;  // i64.store
        case 0x38:
            memcpy(maddr, &sval->value.f32, 4);
            break;  // f32.store
        case 0x39:
            memcpy(maddr, &sval->value.f64, 8);
            break;  // f64.store
        case 0x3a:
            memcpy(maddr, &sval->value.uint32, 1);
            break;  // i32.store8
        case 0x3b:
            memcpy(maddr, &sval->value.uint32, 2);
            break;  // i32.store16
        case 0x3c:
            memcpy(maddr, &sval->value.uint64, 1);
            break;  // i64.store8
        case 0x3d:
            memcpy(maddr, &sval->value.uint64, 2);
            break;  // i64.store16
        case 0x3e:
            memcpy(maddr, &sval->value.uint64, 4);
            break;  // i64.store32
    }
    return true;
}

/**
 * 0x41...0x44 const
 */
bool i_instr_const(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    StackValue target = m->stack[++m->sp];

    switch (opcode) {
        case 0x41:  // i32.const
            target.value_type = I32;
            target.value.uint32 = read_LEB_signed(m->bytes, &m->pc, 32);
            break;
        case 0x42:  // i64.const
            target.value_type = I64;
            target.value.int64 = read_LEB_signed(m->bytes, &m->pc, 64);
            break;
        case 0x43:  // f32.const
            target.value_type = F32;
            memcpy(&target.value.uint32, m->bytes + m->pc, 4);
            m->pc += 4;
            break;
        case 0x44:  // f64.const
            target.value_type = F64;
            memcpy(&target.value.uint64, m->bytes + m->pc, 8);
            m->pc += 8;
            break;
    }
    return true;
}

/**
 * 0x45 eqz
 */
bool i_instr_unairy_u32(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    switch (opcode) {
        case 0x45:  // i32.eqz
            m->stack[m->sp].value.uint32 = m->stack[m->sp].value.uint32 == 0;
            break;
        case 0x50:  // i64.eqz
            m->stack[m->sp].value_type = I32;
            m->stack[m->sp].value.uint32 = m->stack[m->sp].value.uint64 == 0;
            break;
    }
    return true;
}

/**
 * 0x0d binop32
 */
bool i_instr_math_u32(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    uint32_t a = m->stack[m->sp - 1].value.uint32;
    uint32_t b = m->stack[m->sp].value.uint32;
    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x46:
            c = a == b;
            break;  // i32.eq
        case 0x47:
            c = a != b;
            break;  // i32.ne
        case 0x48:
            c = (int32_t)a < (int32_t)b;
            break;  // i32.lt_s
        case 0x49:
            c = a < b;
            break;  // i32.lt_u
        case 0x4a:
            c = (int32_t)a > (int32_t)b;
            break;  // i32.gt_s
        case 0x4b:
            c = a > b;
            break;  // i32.gt_u
        case 0x4c:
            c = (int32_t)a <= (int32_t)b;
            break;  // i32.le_s
        case 0x4d:
            c = a <= b;
            break;  // i32.le_u
        case 0x4e:
            c = (int32_t)a >= (int32_t)b;
            break;  // i32.ge_s
        case 0x4f:
            c = a >= b;
            break;  // i32.ge_u
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

/**
 * 0x0d binop64
 */
bool i_instr_math_u64(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    uint64_t d = m->stack[m->sp - 1].value.uint64;
    uint64_t e = m->stack[m->sp].value.uint64;
    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x51:
            c = d == e;
            break;  // i64.eq
        case 0x52:
            c = d != e;
            break;  // i64.ne
        case 0x53:
            c = (int64_t)d < (int64_t)e;
            break;  // i64.lt_s
        case 0x54:
            c = d < e;
            break;  // i64.lt_u
        case 0x55:
            c = (int64_t)d > (int64_t)e;
            break;  // i64.gt_s
        case 0x56:
            c = d > e;
            break;  // i64.gt_u
        case 0x57:
            c = (int64_t)d <= (int64_t)e;
            break;  // i64.le_s
        case 0x58:
            c = d <= e;
            break;  // i64.le_u
        case 0x59:
            c = (int64_t)d >= (int64_t)e;
            break;  // i64.ge_s
        case 0x5a:
            c = d >= e;
            break;  // i64.ge_u
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

/**
 * 0x0d binop64
 */
bool i_instr_math_f32(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    float g = m->stack[m->sp - 1].value.f32;
    float h = m->stack[m->sp].value.f32;
    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x5b:
            c = g == h;
            break;  // f32.eq
        case 0x5c:
            c = g != h;
            break;  // f32.ne
        case 0x5d:
            c = g < h;
            break;  // f32.lt
        case 0x5e:
            c = g > h;
            break;  // f32.gt
        case 0x5f:
            c = g <= h;
            break;  // f32.le
        case 0x60:
            c = g >= h;
            break;  // f32.ge
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

/**
 * 0x0d binopf64
 */
bool i_instr_math_f64(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    double j = m->stack[m->sp - 1].value.f64;
    double k = m->stack[m->sp].value.f64;

    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x61:
            c = j == k;
            break;  // f64.eq
        case 0x62:
            c = j != k;
            break;  // f64.ne
        case 0x63:
            c = j < k;
            break;  // f64.lt
        case 0x64:
            c = j > k;
            break;  // f64.gt
        case 0x65:
            c = j <= k;
            break;  // f64.le
        case 0x66:
            c = j >= k;
            break;  // f64.ge
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

bool i_instr_unairy_i32(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    uint32_t a = m->stack[m->sp].value.uint32;
    uint32_t c;
    switch (opcode) {
        case 0x67:
            c = a == 0 ? 32 : __builtin_clz(a);
            break;  // i32.clz
        case 0x68:
            c = a == 0 ? 32 : __builtin_ctz(a);
            break;  // i32.ctz
        case 0x69:
            c = __builtin_popcount(a);
            break;  // i32.popcnt
    }
    m->stack[m->sp].value.uint32 = c;
    return true;
}

bool i_instr_unairy_i64(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    uint64_t d = m->stack[m->sp].value.uint64;
    uint64_t f;
    switch (opcode) {
        case 0x79:
            f = d == 0 ? 64 : __builtin_clzll(d);
            break;  // i64.clz
        case 0x7a:
            f = d == 0 ? 64 : __builtin_ctzll(d);
            break;  // i64.ctz
        case 0x7b:
            f = __builtin_popcountll(d);
            break;  // i64.popcnt
    }
    m->stack[m->sp].value.uint64 = f;
    return true;
}

/**
 * 0x0d XXX
 */
bool i_instr_unairy_floating(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    switch (opcode) {
            // unary f32
        case 0x8b:
            m->stack[m->sp].value.f32 = fabs(m->stack[m->sp].value.f32);
            break;  // f32.abs
        case 0x8c:
            m->stack[m->sp].value.f32 = -m->stack[m->sp].value.f32;
            break;  // f32.neg
        case 0x8d:
            m->stack[m->sp].value.f32 = ceil(m->stack[m->sp].value.f32);
            break;  // f32.ceil
        case 0x8e:
            m->stack[m->sp].value.f32 = floor(m->stack[m->sp].value.f32);
            break;  // f32.floor
        case 0x8f:
            m->stack[m->sp].value.f32 = trunc(m->stack[m->sp].value.f32);
            break;  // f32.trunc
        case 0x90:
            m->stack[m->sp].value.f32 = rint(m->stack[m->sp].value.f32);
            break;  // f32.nearest
        case 0x91:
            m->stack[m->sp].value.f32 = sqrt(m->stack[m->sp].value.f32);
            break;  // f32.sqrt

            // unary f64
        case 0x99:
            m->stack[m->sp].value.f64 = fabs(m->stack[m->sp].value.f64);
            break;  // f64.abs
        case 0x9a:
            m->stack[m->sp].value.f64 = -m->stack[m->sp].value.f64;
            break;  // f64.neg
        case 0x9b:
            m->stack[m->sp].value.f64 = ceil(m->stack[m->sp].value.f64);
            break;  // f64.ceil
        case 0x9c:
            m->stack[m->sp].value.f64 = floor(m->stack[m->sp].value.f64);
            break;  // f64.floor
        case 0x9d:
            m->stack[m->sp].value.f64 = trunc(m->stack[m->sp].value.f64);
            break;  // f64.trunc
        case 0x9e:
            m->stack[m->sp].value.f64 = rint(m->stack[m->sp].value.f64);
            break;  // f64.nearest
        case 0x9f:
            m->stack[m->sp].value.f64 = sqrt(m->stack[m->sp].value.f64);
            break;  // f64.sqrt
    }
    return true;
}

/**
 * 0x0d binary_i32
 */
bool i_instr_binary_i32(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    uint32_t a = m->stack[m->sp - 1].value.uint32;
    uint32_t b = m->stack[m->sp].value.uint32;
    uint32_t c;
    m->sp -= 1;
    if (opcode >= 0x6d && opcode <= 0x70 && b == 0) {
        sprintf(exception, "integer divide by zero");
        return false;
    }
    switch (opcode) {
        // case 0x6a: o = __builtin_add_overflow(a, b, &c); break;
        // // i32.add case 0x6b: o = __builtin_sub_overflow(a, b,
        // &c); break;  // i32.sub
        case 0x6a:
            c = a + b;
            break;  // i32.add
        case 0x6b:
            c = a - b;
            break;  // i32.sub
        case 0x6c:
            c = a * b;
            break;  // i32.mul
        case 0x6d:
            if (a == 0x80000000 && b == (uint32_t)-1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            c = (int32_t)a / (int32_t)b;
            break;  // i32.div_s
        case 0x6e:
            c = a / b;
            break;  // i32.div_u
        case 0x6f:
            if (a == 0x80000000 && b == (uint32_t)-1) {
                c = 0;
            } else {
                c = (int32_t)a % (int32_t)b;
            };
            break;  // i32.rem_s
        case 0x70:
            c = a % b;
            break;  // i32.rem_u
        case 0x71:
            c = a & b;
            break;  // i32.and
        case 0x72:
            c = a | b;
            break;  // i32.or
        case 0x73:
            c = a ^ b;
            break;  // i32.xor
        case 0x74:
            c = a << b;
            break;  // i32.shl
        case 0x75:
            c = (int32_t)a >> b;
            break;  // i32.shr_s
        case 0x76:
            c = a >> b;
            break;  // i32.shr_u
        case 0x77:
            c = rotl32(a, b);
            break;  // i32.rotl
        case 0x78:
            c = rotr32(a, b);
            break;  // i32.rotr
    }
    // if (o == 1) {
    //    sprintf(exception, "integer overflow");
    //    return false;
    //}
    m->stack[m->sp].value.uint32 = c;
    return true;
}

/**
 * 0x0d XXX
 */
bool i_instr_binary_i64(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    uint64_t d = m->stack[m->sp - 1].value.uint64;
    uint64_t e = m->stack[m->sp].value.uint64;
    uint64_t f;
    m->sp -= 1;
    if (opcode >= 0x7f && opcode <= 0x82 && e == 0) {
        sprintf(exception, "integer divide by zero");
        return false;
    }
    switch (opcode) {
        case 0x7c:
            f = d + e;
            break;  // i64.add
        case 0x7d:
            f = d - e;
            break;  // i64.sub
        case 0x7e:
            f = d * e;
            break;  // i64.mul
        case 0x7f:
            if (d == 0x8000000000000000 && e == (uint32_t)-1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            f = (int64_t)d / (int64_t)e;
            break;  // i64.div_s
        case 0x80:
            f = d / e;
            break;  // i64.div_u
        case 0x81:
            if (d == 0x8000000000000000 && e == (uint32_t)-1) {
                f = 0;
            } else {
                f = (int64_t)d % (int64_t)e;
            }
            break;  // i64.rem_s
        case 0x82:
            f = d % e;
            break;  // i64.rem_u
        case 0x83:
            f = d & e;
            break;  // i64.and
        case 0x84:
            f = d | e;
            break;  // i64.or
        case 0x85:
            f = d ^ e;
            break;  // i64.xor
        case 0x86:
            f = d << e;
            break;  // i64.shl
        case 0x87:
            f = ((int64_t)d) >> e;
            break;  // i64.shr_s
        case 0x88:
            f = d >> e;
            break;  // i64.shr_u
        case 0x89:
            f = rotl64(d, e);
            break;  // i64.rotl
        case 0x8a:
            f = rotr64(d, e);
            break;  // i64.rotr
    }
    m->stack[m->sp].value.uint64 = f;

    return true;
}

/**
 * 0x0d XXX
 */
bool i_instr_binary_f32(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    float g = m->stack[m->sp - 1].value.f32;
    float h = m->stack[m->sp].value.f32;
    float i;
    m->sp -= 1;
    switch (opcode) {
        case 0x92:
            i = g + h;
            break;  // f32.add
        case 0x93:
            i = g - h;
            break;  // f32.sub
        case 0x94:
            i = g * h;
            break;  // f32.mul
        case 0x95:
            i = g / h;
            break;  // f32.div
        case 0x96:
            i = wa_fmin(g, h);
            break;  // f32.min
        case 0x97:
            i = wa_fmax(g, h);
            break;  // f32.max
        case 0x98:
            i = signbit(h) ? -fabs(g) : fabs(g);
            break;  // f32.copysign
    }
    m->stack[m->sp].value.f32 = i;
    return true;
}

/**
 * 0x0d XXX
 */
bool i_instr_binary_f64(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    double j = m->stack[m->sp - 1].value.f64;
    double k = m->stack[m->sp].value.f64;
    double l;
    m->sp -= 1;
    switch (opcode) {
        case 0xa0:
            l = j + k;
            break;  // f64.add
        case 0xa1:
            l = j - k;
            break;  // f64.sub
        case 0xa2:
            l = j * k;
            break;  // f64.mul
        case 0xa3:
            l = j / k;
            break;  // f64.div
        case 0xa4:
            l = wa_fmin(j, k);
            break;  // f64.min
        case 0xa5:
            l = wa_fmax(j, k);
            break;  // f64.max
        case 0xa6:
            l = signbit(k) ? -fabs(j) : fabs(j);
            break;  // f64.copysign
    }
    m->stack[m->sp].value.f64 = l;

    return true;
}

/**
 * 0x0d XXX
 */
bool i_instr_conversion(Module *m, uint32_t *cur_pc, uint8_t opcode) {
    switch (opcode) {
        case 0xa7:
            m->stack[m->sp].value.uint64 &= 0x00000000ffffffff;
            m->stack[m->sp].value_type = I32;
            break;  // i32.wrap/i64
        case 0xa8:
            if (isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f32 >= INT32_MAX ||
                       m->stack[m->sp].value.f32 < INT32_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.int32 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_s/f32
        case 0xa9:
            if (isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f32 >= UINT32_MAX ||
                       m->stack[m->sp].value.f32 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.uint32 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_u/f32
        case 0xaa:
            if (isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f64 > INT32_MAX ||
                       m->stack[m->sp].value.f64 < INT32_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.int32 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_s/f64
        case 0xab:
            if (isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f64 > UINT32_MAX ||
                       m->stack[m->sp].value.f64 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.uint32 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_u/f64
        case 0xac:
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.uint32;
            sext_32_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.extend_s/i32
        case 0xad:
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.uint32;
            m->stack[m->sp].value_type = I64;
            break;  // i64.extend_u/i32
        case 0xae:
            if (isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f32 >= INT64_MAX ||
                       m->stack[m->sp].value.f32 < INT64_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.int64 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_s/f32
        case 0xaf:
            if (isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f32 >= UINT64_MAX ||
                       m->stack[m->sp].value.f32 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_u/f32
        case 0xb0:
            if (isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f64 >= INT64_MAX ||
                       m->stack[m->sp].value.f64 < INT64_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.int64 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_s/f64
        case 0xb1:
            if (isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (m->stack[m->sp].value.f64 >= UINT64_MAX ||
                       m->stack[m->sp].value.f64 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_u/f64
        case 0xb2:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.int32;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_s/i32
        case 0xb3:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.uint32;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_u/i32
        case 0xb4:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.int64;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_s/i64
        case 0xb5:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.uint64;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_u/i64
        case 0xb6:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = F32;
            break;  // f32.demote/f64
        case 0xb7:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.int32;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_s/i32
        case 0xb8:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.uint32;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_u/i32
        case 0xb9:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.int64;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_s/i64
        case 0xba:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.uint64;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_u/i64
        case 0xbb:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = F64;
            break;  // f64.promote/f32

            // reinterpretations
        case 0xbc:
            m->stack[m->sp].value_type = I32;
            break;  // i32.reinterpret/f32
        case 0xbd:
            m->stack[m->sp].value_type = I64;
            break;  // i64.reinterpret/f64
        case 0xbe:  // memmove(&m->stack[m->sp].value.f32,
                    // &m->stack[m->sp].value.uint32, 4);
            m->stack[m->sp].value_type = F32;
            break;  // f32.reinterpret/i32
        case 0xbf:
            m->stack[m->sp].value_type = F64;
            break;  // f64.reinterpret/i64
    }

    return true;
}

// End Control Instructions

bool interpret(Module *m) {
    uint8_t *bytes = m->bytes;

    uint32_t cur_pc;
    uint8_t opcode;
    bool success = true;

    while (success && m->pc < m->byte_count) {
        opcode = bytes[m->pc];
        cur_pc = m->pc;
        m->pc += 1;

        if (TRACE) {
            if (DEBUG) {
                dump_stacks(m);
            }
            info(" PC:  0x%x OPCODE: <0x%x>\n", cur_pc, opcode);
        }

        switch (opcode) {
            //
            // Control flow operators
            //
            case 0x00:  // unreachable
                sprintf(exception, "%s", "unreachable");
                success &= false;
            case 0x01:  // nop
                continue;
            case 0x02:  // block
                success &= i_instr_block(m, &cur_pc);
                continue;
            case 0x03:  // loop
                success &= i_instr_loop(m, &cur_pc);
                continue;
            case 0x04:  // if
                success &= i_instr_if(m, &cur_pc);
                continue;
            case 0x05:  // else
                success &= i_instr_else(m, &cur_pc);
                continue;
            case 0x0b:  // end
                success &= i_instr_end(m, &cur_pc);
                continue;
            case 0x0c:  // br
                success &= i_instr_br(m, &cur_pc);
                continue;
            case 0x0d:  // br_if
                success &= i_instr_br_if(m, &cur_pc);
                continue;
            case 0x0e:  // br_table
                success &= i_instr_br_table(m, &cur_pc);
                continue;
            case 0x0f:  // return
                success &= i_instr_return(m, &cur_pc);
                continue;

                //
                // Call operators
                //
            case 0x10: {  // call
                success &= i_instr_call(m, &cur_pc);
                continue;
            }
            case 0x11:  // call_indirect
                success &= i_instr_call_indirect(m, &cur_pc);
                continue;

                //
                // Parametric operators
                //
            case 0x1a:  // drop
                success &= i_instr_drop(m, &cur_pc);
                continue;
            case 0x1b:  // select
                success &= i_instr_select(m, &cur_pc);
                continue;

                //
                // Variable access
                //
            case 0x20:  // get_local
                success &= i_instr_get_local(m, &cur_pc);
                continue;
            case 0x21:  // set_local
                success &= i_instr_set_local(m, &cur_pc);
                continue;
            case 0x22:  // tee_local
                success &= i_instr_tee_local(m, &cur_pc);
                continue;
            case 0x23:  // get_global
                success &= i_instr_tee_local(m, &cur_pc);
                continue;
            case 0x24:  // set_global
                success &= i_instr_set_global(m, &cur_pc);
                continue;

                //
                // Memory-related operators
                //
            case 0x3f:  // current_memory
                success &= i_instr_current_memory(m, &cur_pc);
                continue;
            case 0x40:  // grow_memory
                success &= i_instr_grow_memory(m, &cur_pc);
                continue;
                // Memory load operators
            case 0x28 ... 0x35:
                success &= i_instr_mem_load(m, &cur_pc, opcode);
                continue;
                // Memory store operators
            case 0x36 ... 0x3e:
                success &= i_instr_mem_store(m, &cur_pc, opcode);
                continue;

                //
                // Constants
                //
            case 0x41 ... 0x44:  // i32.const
                success &= i_instr_const(m, &cur_pc, opcode);
                continue;

                //
                // Comparison operators
                //

                // unary
            case 0x45:  // i32.eqz
            case 0x50:  // i64.eqz
                success &= i_instr_unairy_u32(m, &cur_pc, opcode);
                continue;

                // i32 binary
            case 0x46 ... 0x4f:
                success &= i_instr_math_u32(m, &cur_pc, opcode);
                continue;
            case 0x51 ... 0x5a:
                success &= i_instr_math_u64(m, &cur_pc, opcode);
                continue;
            case 0x5b ... 0x60:
                success &= i_instr_math_f32(m, &cur_pc, opcode);
                continue;
            case 0x61 ... 0x66:
                success &= i_instr_math_f64(m, &cur_pc, opcode);
                continue;

                //
                // Numeric operators
                //

                // unary i32
            case 0x67 ... 0x69:
                success &= i_instr_unairy_i32(m, &cur_pc, opcode);
                continue;

                // unary i64
            case 0x79 ... 0x7b:
                success &= i_instr_unairy_i64(m, &cur_pc, opcode);
                continue;

            case 0x8b ... 0x91:  // unary f32
            case 0x99 ... 0x9f:  // unary f64
                success &= i_instr_unairy_floating(m, &cur_pc, opcode);
                continue;

                // i32 binary
            case 0x6a ... 0x78:
                success &= i_instr_binary_i32(m, &cur_pc, opcode);
                continue;

                // i64 binary
            case 0x7c ... 0x8a:
                success &= i_instr_binary_i64(m, &cur_pc, opcode);
                continue;

                // f32 binary
            case 0x92 ... 0x98:
                success &= i_instr_binary_f32(m, &cur_pc, opcode);
                continue;

                // f64 binary
            case 0xa0 ... 0xa6:
                success &= i_instr_binary_f64(m, &cur_pc, opcode);
                continue;

                // conversion operations
            case 0xa7 ... 0xbb:
                success &= i_instr_conversion(m, &cur_pc, opcode);
                continue;
            default:
                sprintf(exception, "unrecognized opcode 0x%x", opcode);
                return false;
        }
    }
    ASSERT(!success, "While loop broken unexpectedly!");
    return false;  // We shouldn't reach here
}

void run_init_expr(Module *m, uint8_t type, uint32_t *pc) {
    // Run the init_expr
    Block block;
    block.block_type = 0x01;
    block.type = get_block_type(type);
    block.start_addr = *pc;

    m->pc = *pc;
    push_block(m, &block, m->sp);
    // WARNING: running code here to get initial value!
    info("  running init_expr at 0x%x: %s\n", m->pc, block_repr(&block));
    interpret(m);
    *pc = m->pc;

    ASSERT(m->stack[m->sp].value_type == type,
           "init_expr type mismatch 0x%x != 0x%x", m->stack[m->sp].value_type,
           type);
}

//
// Public API
//
uint32_t WARDuino::get_export_fidx(Module *m, char *name) {
    // Find name function index
    for (uint32_t f = 0; f < m->function_count; f++) {
        char *fname = m->functions[f].export_name;
        if (!fname) {
            continue;
        }
        if (strncmp(name, fname, 1024) == 0) {
            return f;
        }
    }
    return -1;
}

Module *WARDuino::load_module(uint8_t *bytes, uint32_t byte_count,
                              Options options) {
    debug("Loading module of size %d \n", byte_count);
    uint8_t vt;
    uint32_t pos = 0, word;
    Module *m;
    // Allocate the module
    m = (Module *)acalloc(1, sizeof(Module), "Module");
    m->options = options;
    // Empty stacks
    m->sp = -1;
    m->fp = -1;
    m->csp = -1;

    m->bytes = bytes;
    m->byte_count = byte_count;
    m->block_lookup = (Block **)acalloc(m->byte_count, sizeof(Block *),
                                        "function->block_lookup");
    m->start_function = -1;

    // Check the module
    pos = 0;
    word = read_uint32(bytes, &pos);
    debug("Magic number is 0x%x\n", word);
    ASSERT(word == WA_MAGIC, "Wrong module magic 0x%x\n", word);
    word = read_uint32(bytes, &pos);
    ASSERT(word == WA_VERSION, "Wrong module version 0x%x\n", word);
    // Read the sections
    while (pos < byte_count) {
        uint32_t id = read_LEB(bytes, &pos, 7);
        uint32_t slen = read_LEB(bytes, &pos, 32);
        int start_pos = pos;
        debug("Reading section %d at 0x%x, length %d\n", id, pos, slen);
        switch (id) {
            case 0: {
                warn("Parsing Custom(0) section (length: 0x%x)\n", slen);
                uint32_t end_pos = pos + slen;
                char *name = read_string(bytes, &pos, NULL);
                warn("  Section name '%s'\n", name);
                if (strncmp(name, "dylink", 7) == 0) {
                    // https://github.com/WebAssembly/tool-conventions/blob/master/DynamicLinking.md
                    // TODO: make use of these
                    uint32_t memorysize = read_LEB(bytes, &pos, 32);
                    uint32_t tablesize = read_LEB(bytes, &pos, 32);
                    (void)memorysize;
                    (void)tablesize;
                } else {
                    error("Ignoring unknown custom section '%s'\n", name);
                }
                pos = end_pos;
                break;
            }
            case 1:
                warn("Parsing Type(1) section (length: 0x%x)\n", slen);
                m->type_count = read_LEB(bytes, &pos, 32);
                m->types = (Type *)acalloc(m->type_count, sizeof(Type),
                                           "Module->types");

                for (uint32_t c = 0; c < m->type_count; c++) {
                    Type *type = &m->types[c];
                    type->form = read_LEB(bytes, &pos, 7);
                    type->param_count = read_LEB(bytes, &pos, 32);
                    type->params = (uint32_t *)acalloc(
                        type->param_count, sizeof(uint32_t), "type->params");
                    for (uint32_t p = 0; p < type->param_count; p++) {
                        type->params[p] = read_LEB(bytes, &pos, 32);
                    }
                    type->result_count = read_LEB(bytes, &pos, 32);
                    type->results = (uint32_t *)acalloc(
                        type->result_count, sizeof(uint32_t), "type->results");
                    for (uint32_t r = 0; r < type->result_count; r++) {
                        type->results[r] = read_LEB(bytes, &pos, 32);
                    }
                    // TODO: calculate this above and remove get_type_mask
                    type->mask = get_type_mask(type);
                    debug("  form: 0x%x, params: %d, results: %d\n", type->form,
                          type->param_count, type->result_count);
                }
                break;
            case 2: {
                warn("Parsing Import(2) section (length: 0x%x)\n", slen);
                uint32_t import_count = read_LEB(bytes, &pos, 32);
                for (uint32_t gidx = 0; gidx < import_count; gidx++) {
                    uint32_t module_len, field_len;
                    char *import_module = read_string(bytes, &pos, &module_len);
                    char *import_field = read_string(bytes, &pos, &field_len);

                    uint32_t external_kind = bytes[pos++];

                    debug("  import: %d/%d, external_kind: %d, %s.%s\n", gidx,
                          import_count, external_kind, import_module,
                          import_field);

                    uint32_t type_index = 0, fidx;
                    uint8_t content_type = 0, mutability;

                    switch (external_kind) {
                        case 0x00:  // Function
                            type_index = read_LEB(bytes, &pos, 32);
                            break;
                        case 0x01:  // Table
                            parse_table_type(m, &pos);
                            break;
                        case 0x02:  // Memory
                            parse_memory_type(m, &pos);
                            break;
                        case 0x03:  // Global
                            content_type = read_LEB(bytes, &pos, 7);
                            // TODO: use mutability
                            mutability = read_LEB(bytes, &pos, 1);
                            (void)mutability;
                            break;
                    }

                    void *val;
                    char *err,
                        *sym = (char *)malloc(module_len + field_len + 5);

                    // TODO add special case form primitives with resolvePrim
                    do {
                        // Try using module as handle filename
                        if (resolvesym(import_module, import_field,
                                       (Primitive *)&val, &err)) {
                            break;
                        }

                        // Try concatenating module and field using underscores
                        // Also, replace '-' with '_'
                        sprintf(sym, "_%s__%s_", import_module, import_field);
                        int sidx = -1;
                        while (sym[++sidx]) {
                            if (sym[sidx] == '-') {
                                sym[sidx] = '_';
                            }
                        }
                        if (resolvesym(NULL, sym, (Primitive *)&val, &err)) {
                            break;
                        }

                        // If enabled, try without the leading underscore (added
                        // by emscripten for external symbols)
                        if (m->options.dlsym_trim_underscore &&
                            (strncmp("env", import_module, 4) == 0) &&
                            (strncmp("_", import_field, 1) == 0)) {
                            sprintf(sym, "%s", import_field + 1);
                            if (resolvesym(NULL, sym, (Primitive *)&val,
                                           &err)) {
                                break;
                            }
                        }

                        // Try the plain symbol by itself with module
                        // name/handle
                        sprintf(sym, "%s", import_field);
                        if (resolvesym(NULL, sym, (Primitive *)&val, &err)) {
                            break;
                        }

                        FATAL("Error: %s\n", err);
                    } while (false);

                    debug("  found '%s.%s' as symbol '%s' at address %p\n",
                          import_module, import_field, sym, val);
                    free(sym);

                    // Store in the right place
                    switch (external_kind) {
                        case 0x00:  // Function
                        {
                            fidx = m->function_count;
                            m->import_count += 1;
                            m->function_count += 1;
                            m->functions = (Block *)arecalloc(
                                m->functions, fidx, m->import_count,
                                sizeof(Block), "Block(imports)");

                            Block *func = &m->functions[fidx];
                            func->import_module = import_module;
                            func->import_field = import_field;
                            func->type = &m->types[type_index];
                            debug(
                                "  import: %s.%s, fidx: 0x%x, type_index: "
                                "0x%x\n",
                                func->import_module, func->import_field, fidx,
                                type_index);

                            func->func_ptr = (void *(*)())val;
                            break;
                        }
                        case 0x01:  // Table
                        {
                            ASSERT(!m->table.entries,
                                   "More than 1 table not supported\n");
                            Table *tval = (Table *)val;
                            m->table.entries = (uint32_t *)val;
                            ASSERT(m->table.initial <= tval->maximum,
                                   "Imported table is not large enough\n");
                            warn("  setting table.entries to: %p\n",
                                 *(uint32_t **)val);
                            m->table.entries = *(uint32_t **)val;
                            m->table.size = tval->size;
                            m->table.maximum = tval->maximum;
                            m->table.entries = tval->entries;
                            break;
                        }
                        case 0x02:  // Memory
                        {
                            ASSERT(!m->memory.bytes,
                                   "More than 1 memory not supported\n");
                            Memory *mval = (Memory *)val;
                            ASSERT(m->memory.initial <= mval->maximum,
                                   "Imported memory is not large enough\n");
                            warn(
                                "  setting memory pages: %d, max: %d, bytes: "
                                "%p\n",
                                mval->pages, mval->maximum, mval->bytes);
                            m->memory.pages = mval->pages;
                            m->memory.maximum = mval->maximum;
                            m->memory.bytes = mval->bytes;
                            break;
                        }
                        case 0x03:  // Global
                        {
                            m->global_count += 1;
                            m->globals = (StackValue *)arecalloc(
                                m->globals, m->global_count - 1,
                                m->global_count, sizeof(StackValue), "globals");
                            StackValue *glob = &m->globals[m->global_count - 1];
                            glob->value_type = content_type;

                            switch (content_type) {
                                case I32:
                                    memcpy(&glob->value.uint32, val, 4);
                                    break;
                                case I64:
                                    memcpy(&glob->value.uint64, val, 8);
                                    break;
                                case F32:
                                    memcpy(&glob->value.f32, val, 4);
                                    break;
                                case F64:
                                    memcpy(&glob->value.f64, val, 8);
                                    break;
                            }
                            debug(
                                "    setting global %d (content_type %d) to "
                                "%p: %s\n",
                                m->global_count - 1, content_type, val,
                                value_repr(glob));
                            break;
                        }
                        default:
                            FATAL("Import of kind %d not supported\n",
                                  external_kind);
                    }
                }
                break;
            }
            case 3: {
                warn("Parsing Function(3) section (length: 0x%x)\n", slen);
                m->function_count += read_LEB(bytes, &pos, 32);
                debug("  import_count: %d, new count: %d\n", m->import_count,
                      m->function_count);

                Block *functions;
                functions = (Block *)acalloc(m->function_count, sizeof(Block),
                                             "Block(function)");
                if (m->import_count != 0) {
                    memcpy(functions, m->functions,
                           sizeof(Block) * m->import_count);
                }
                m->functions = functions;

                for (uint32_t f = m->import_count; f < m->function_count; f++) {
                    uint32_t tidx = read_LEB(bytes, &pos, 32);
                    m->functions[f].fidx = f;
                    m->functions[f].type = &m->types[tidx];
                    debug("  function fidx: 0x%x, tidx: 0x%x\n", f, tidx);
                }
                break;
            }
            case 4: {
                warn("Parsing Table(4) section\n");
                uint32_t table_count = read_LEB(bytes, &pos, 32);
                debug("  table count: 0x%x\n", table_count);
                ASSERT(table_count == 1, "More than 1 table not supported");
                // Allocate the table
                // for (uint32_t c=0; c<table_count; c++) {
                parse_table_type(m, &pos);
                // If it's not imported then don't mangle it
                m->options.mangle_table_index = false;
                m->table.entries = (uint32_t *)acalloc(
                    m->table.size, sizeof(uint32_t), "Module->table.entries");
                //}
                break;
            }
            case 5: {
                warn("Parsing Memory(5) section\n");
                uint32_t memory_count = read_LEB(bytes, &pos, 32);
                debug("  memory count: 0x%x\n", memory_count);
                ASSERT(memory_count == 1, "More than 1 memory not supported\n");
                // Allocate memory
                // for (uint32_t c=0; c<memory_count; c++) {
                parse_memory_type(m, &pos);
                m->memory.bytes = (uint8_t *)acalloc(
                    m->memory.pages * PAGE_SIZE, sizeof(uint32_t),
                    "Module->memory.bytes");
                //}
                break;
            }
            case 6: {
                warn("Parsing Global(6) section\n");
                uint32_t global_count = read_LEB(bytes, &pos, 32);
                for (uint32_t g = 0; g < global_count; g++) {
                    // Same allocation Import of global above
                    uint8_t type = read_LEB(bytes, &pos, 7);
                    // TODO: use mutability
                    uint8_t mutability = read_LEB(bytes, &pos, 1);
                    (void)mutability;
                    uint32_t gidx = m->global_count;
                    m->global_count += 1;
                    m->globals = (StackValue *)arecalloc(
                        m->globals, gidx, m->global_count, sizeof(StackValue),
                        "globals");
                    m->globals[gidx].value_type = type;

                    // Run the init_expr to get global value
                    run_init_expr(m, type, &pos);

                    m->globals[gidx] = m->stack[m->sp--];
                }
                pos = start_pos + slen;
                break;
            }
            case 7: {
                warn("Parsing Export(7) section (length: 0x%x)\n", slen);
                uint32_t export_count = read_LEB(bytes, &pos, 32);
                for (uint32_t e = 0; e < export_count; e++) {
                    char *name = read_string(bytes, &pos, NULL);

                    uint32_t kind = bytes[pos++];
                    uint32_t index = read_LEB(bytes, &pos, 32);
                    if (kind != 0x00) {
                        warn(
                            "  ignoring non-function export '%s'"
                            " kind 0x%x index 0x%x\n",
                            name, kind, index);
                        continue;
                    }
                    m->functions[index].export_name = name;
                    debug("  export: %s (0x%x)\n", name, index);
                }
                break;
            }
            case 8:
                warn("Parsing Start(8) section (length: 0x%x)\n", slen);
                m->start_function = read_LEB(bytes, &pos, 32);
                break;
            case 9: {
                warn("Parsing Element(9) section (length: 0x%x)\n", slen);
                uint32_t element_count = read_LEB(bytes, &pos, 32);

                for (uint32_t c = 0; c < element_count; c++) {
                    uint32_t index = read_LEB(bytes, &pos, 32);
                    ASSERT(index == 0, "Only 1 default table in MVP");

                    // Run the init_expr to get offset
                    run_init_expr(m, I32, &pos);

                    uint32_t offset = m->stack[m->sp--].value.uint32;

                    if (m->options.mangle_table_index) {
                        // offset is the table address + the index (not sized
                        // for the pointer size) so get the actual (sized) index
                        debug(
                            "   origin offset: 0x%x, table addr: 0x%x, new "
                            "offset: 0x%x\n",
                            offset, (uint32_t)((uint64_t)m->table.entries),
                            offset - (uint32_t)((uint64_t)m->table.entries));
                        // offset = offset -
                        // (uint32_t)((uint64_t)m->table.entries & 0xFFFFFFFF);
                        offset =
                            offset - (uint32_t)((uint64_t)m->table.entries);
                    }

                    uint32_t num_elem = read_LEB(bytes, &pos, 32);
                    warn("  table.entries: %p, offset: 0x%x\n",
                         m->table.entries, offset);
                    if (!m->options.disable_memory_bounds) {
                        ASSERT(offset + num_elem <= m->table.size,
                               "table overflow %d+%d > %d\n", offset, num_elem,
                               m->table.size);
                    }
                    for (uint32_t n = 0; n < num_elem; n++) {
                        debug(
                            "  write table entries %p, offset: 0x%x, n: 0x%x, "
                            "addr: %p\n",
                            m->table.entries, offset, n,
                            &m->table.entries[offset + n]);
                        m->table.entries[offset + n] =
                            read_LEB(bytes, &pos, 32);
                    }
                }
                pos = start_pos + slen;
                break;
                // 9 and 11 are similar so keep them together, 10 is below 11
            }
            case 11: {
                warn("Parsing Data(11) section (length: 0x%x)\n", slen);
                uint32_t seg_count = read_LEB(bytes, &pos, 32);
                for (uint32_t s = 0; s < seg_count; s++) {
                    uint32_t midx = read_LEB(bytes, &pos, 32);
                    ASSERT(midx == 0, "Only 1 default memory in MVP");

                    // Run the init_expr to get the offset
                    run_init_expr(m, I32, &pos);

                    uint32_t offset = m->stack[m->sp--].value.uint32;

                    // Copy the data to the memory offset
                    uint32_t size = read_LEB(bytes, &pos, 32);
                    if (!m->options.disable_memory_bounds) {
                        ASSERT(offset + size <= m->memory.pages * PAGE_SIZE,
                               "memory overflow %d+%d > %d\n", offset, size,
                               (uint32_t)(m->memory.pages * PAGE_SIZE));
                    }
                    info(
                        "  setting 0x%x bytes of memory at 0x%p + offset "
                        "0x%x\n",
                        size, m->memory.bytes, offset);
                    memcpy(m->memory.bytes + offset, bytes + pos, size);
                    pos += size;
                }

                break;
            }
            case 10: {
                warn("Parsing Code(10) section (length: 0x%x)\n", slen);
                uint32_t body_count = read_LEB(bytes, &pos, 32);
                for (uint32_t b = 0; b < body_count; b++) {
                    Block *function = &m->functions[m->import_count + b];
                    uint32_t body_size = read_LEB(bytes, &pos, 32);
                    uint32_t payload_start = pos;
                    uint32_t local_count = read_LEB(bytes, &pos, 32);
                    uint32_t save_pos, tidx, lidx, lecount;

                    // Local variable handling

                    // Get number of locals for alloc
                    save_pos = pos;
                    function->local_count = 0;
                    for (uint32_t l = 0; l < local_count; l++) {
                        lecount = read_LEB(bytes, &pos, 32);
                        function->local_count += lecount;
                        tidx = read_LEB(bytes, &pos, 7);
                        (void)tidx;  // TODO: use tidx?
                    }

                    if (function->local_count > 0) {
                        function->locals = (uint32_t *)acalloc(
                            function->local_count, sizeof(uint32_t),
                            "function->locals");
                    }

                    // Restore position and read the locals
                    pos = save_pos;
                    lidx = 0;
                    for (uint32_t l = 0; l < local_count; l++) {
                        lecount = read_LEB(bytes, &pos, 32);
                        vt = read_LEB(bytes, &pos, 7);
                        for (uint32_t l = 0; l < lecount; l++) {
                            function->locals[lidx++] = vt;
                        }
                    }

                    function->start_addr = pos;
                    function->end_addr = payload_start + body_size - 1;
                    function->br_addr = function->end_addr;
                    ASSERT(bytes[function->end_addr] == 0x0b,
                           "Code section did not end with 0x0b\n");
                    pos = function->end_addr + 1;
                }
                break;
            }
            default:
                FATAL("Section %d unimplemented\n", id);
                pos += slen;
        }
    }

    find_blocks(m);

    if (m->start_function != (uint32_t)-1) {
        uint32_t fidx = m->start_function;
        bool result;
        warn("Running start function 0x%x ('%s')\n", fidx,
             m->functions[fidx].export_name);

        if (TRACE && DEBUG) {
            dump_stacks(m);
        }

        if (fidx < m->import_count) {
            // THUNK thunk_out(m, fidx);     // import/thunk call
        } else {
            setup_call(m, fidx);  // regular function call
        }

        if (m->csp < 0) {
            // start function was a direct external call
            result = true;
        } else {
            // run the function setup by setup_call
            result = interpret(m);
        }
        if (!result) {
            FATAL("Exception: %s\n", exception);
        }
    }

    return m;
}

// if entry == NULL,  attempt to invoke 'main' or '_main'
// Return value of false means exception occured
bool WARDuino::invoke(Module *m, uint32_t fidx) {
    bool result;
    if (TRACE && DEBUG) {
        dump_stacks(m);
    }
    setup_call(m, fidx);
    result = interpret(m);
    if (TRACE && DEBUG) {
        dump_stacks(m);
    }
    return result;
}

int WARDuino::run_module(uint8_t *bytes, int size) {
    Options opts;
    initTypes();
    install_primitives();

    Module *m = load_module(bytes, size, opts);
    return m->function_count;
}
