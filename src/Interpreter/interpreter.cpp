#include "interpreter.h"

#include <cmath>
#include <cstring>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/CallbackHandler.h"
#include "instructions.h"

void Interpreter::push_block(Module *m, Block *block, int sp) {
    m->csp += 1;
    m->callstack[m->csp].block = block;
    m->callstack[m->csp].sp = sp;
    m->callstack[m->csp].fp = m->fp;
    m->callstack[m->csp].ra_ptr = m->pc_ptr;
}

Block *Interpreter::pop_block(Module *m) {
    Frame *frame = &m->callstack[m->csp--];
    Type *t = frame->block->type;

    if (frame->block->block_type == 0xff) {
        CallbackHandler::resolving_event = false;
        // free if event guard
        free(frame->block);
        frame->block = nullptr;
        frame = &m->callstack[m->csp--];
        t = frame->block->type;
    }

    if (frame->block->block_type == 0xfe) {
        m->warduino->program_state = PROXYhalt;
        m->warduino->debugger->sendProxyCallResult(m);
        // free if proxy guard
        free(frame->block);
        frame->block = nullptr;
        frame = &m->callstack[m->csp--];
        t = frame->block->type;
    }

    // TODO: validate return value if there is one

    m->fp = frame->fp;  // Restore frame pointer

    // Validate the return value
    if (t->result_count == 1) {
        if (m->stack[m->sp].value_type != t->results[0]) {
            sprintf(exception, "call type mismatch");
            return nullptr;
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
        m->pc_ptr = frame->ra_ptr;
    }

    return frame->block;
}

void Interpreter::setup_call(Module *m, uint32_t fidx) {
    Block *func = &m->functions[fidx];
    Type *type = func->type;

    // Push current frame on the call stack
    push_block(m, func, m->sp - type->param_count);

#if TRACE
    dbg_warn("  >> fn0x%x(%arg0_64) %s(", fidx, fidx,
             func->export_name
                 ? func->export_name
                 : "") for (int p = ((int)type->param_count) - 1; p >= 0; p--) {
        dbg_warn("%s%s", value_repr(&m->stack[m->sp - p]), p ? " " : "");
    }
    dbg_warn("), %arg0_64 locals, %arg0_64 results\n", func->local_count,
             type->result_count);
#endif

    // Push locals (dropping extras)
    m->fp = m->sp - ((int)type->param_count) + 1;
    // TODO: validate arguments vs formal params

    // Push function locals
    for (uint32_t lidx = 0; lidx < func->local_count; lidx++) {
        m->sp += 1;
        m->stack[m->sp].value_type = func->local_value_type[lidx];
        m->stack[m->sp].value = {0};  // Initialize whole union to 0
    }

    // Set program counter to start of function
    m->pc_ptr = func->start_ptr;
}

uint32_t LOAD_SIZE[] = {4, 8, 4, 8, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4};
uint32_t LOAD_TYPES[] = {I32, I64, F32, F64, I32, I32, I32,
                         I32, I64, I64, I64, I64, I64, I64};
uint32_t STORE_SIZE[] = {4, 8, 4, 8, 1, 2, 1, 2, 4};

bool Interpreter::store(Module *m, uint8_t type, uint32_t addr,
                        StackValue &sval) {
    if (m->warduino->debugger->isProxy()) {
        return m->warduino->debugger;
    }

    uint8_t *maddr, *mem_end;
    uint32_t size = STORE_SIZE[abs(type - I32)];
    bool overflow = false;

    maddr = m->memory.bytes + addr;
    if (maddr < m->memory.bytes) {
        overflow = true;
    }
    mem_end = m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + size > mem_end) {
        overflow = true;
    }

    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            report_overflow(m, maddr);
            return false;
        }
    }

    memcpy(maddr, &sval.value, size);
    return true;
}

bool Interpreter::load(Module *m, uint8_t type, uint32_t addr,
                       uint32_t offset = 0) {
    bool overflow = false;
    if (offset + addr < addr) {
        overflow = true;
    }

    uint8_t *maddr = m->memory.bytes + addr + offset;
    uint32_t size = LOAD_SIZE[abs(type - I32)];
    uint8_t *mem_end = m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;

    overflow |= maddr < m->memory.bytes || maddr + size > mem_end;

    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            report_overflow(m, maddr);
            return false;
        }
    }

    m->stack[++m->sp].value.uint64 = 0;  // initialize to 0

    memcpy(&m->stack[m->sp].value, maddr, size);
    m->stack[m->sp].value_type = LOAD_TYPES[abs(type - I32)];

    switch (type) {
        case I32_8_s:
            sext_8_32(&m->stack[m->sp].value.uint32);
            break;
        case I32_16_s:
            sext_16_32(&m->stack[m->sp].value.uint32);
            break;
        case I64_8_s:
            sext_8_64(&m->stack[m->sp].value.uint64);
            break;
        case I64_16_s:
            sext_16_64(&m->stack[m->sp].value.uint64);
            break;
        case I64_32_s:
            sext_32_64(&m->stack[m->sp].value.uint64);
            break;
        default:
            break;
    }
    return true;
}

void Interpreter::threadedInterpreter(Module *m) {
    /*

        -- DATA needed for executing instructions --

    */
    // instruction states and local variables
    uint8_t opcode;
    uint8_t *block_ptr;
    StackValue *target;

    // TODO check whether we can don't need all these variables with the correct
    // type
    //  int32_t stack variables (shouldn't they be pointer to the stack?)
    int32_t arg0;
    int32_t arg1;
    int32_t result;

    uint32_t arg0_uint;
    uint32_t arg1_uint;
    uint32_t result_uint;

    // doubles
    double arg0_d;
    double arg1_d;
    double result_d;
    // floats
    float arg0_f;
    float arg1_f;
    // int64_t stack variables
    float fresult;
    uint64_t arg0_64;
    uint64_t arg1_64;

    uint64_t arg0_int64_t;
    uint64_t arg1_int64_t;
    uint64_t result_uint64_t;

    uint32_t flags;
    uint32_t offset;
    uint32_t addr;

    // Block pointers
    Block *block = nullptr;
    uint32_t fidx;
    std::__1::map<uint8_t *, Block *>::iterator block_itr;

    // Jump table for instruction dispatch
    void *jumpTable[256] = {0};

    // set all instructions to undefined
    for (int i = 0; i < 256; i++) {
        jumpTable[i] = &&undefinedInstruction;
    }

    goto initJumpTable;

    /*

        -- INSTRUCTIONS --

    */

    /**
     * 0x24 set_global
     */
i_instr_set_global:
    arg0 = read_LEB_32(&m->pc_ptr);
    m->globals[arg0] = m->stack[m->sp--];
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg, value_repr(&m->stack[m->sp + 1]));
#endif
    goto nextInstruction;

/**
 * 0x11 call_indirect
 */
i_instr_call_indirect:

    // uint32_t tidx =
    read_LEB_32(&m->pc_ptr);  // TODO: use tidx?
                              /// (void)tidx;

    read_LEB(&m->pc_ptr, 1);  // reserved immediate
    arg0 = m->stack[m->sp--].value.uint32;
    if (m->options.mangle_table_index) {
        // val is the table address + the index (not sized for the
        // pointer size) so get the actual (sized) index
#if TRACE
        debug("      - entries: %p, original val: 0x%x, new val: 0x%x\n",
              m->table.entries, val,
              (uint32_t)((uint64_t)m->table.entries) - val);
#endif
        // val = val - (uint32_t)((uint64_t)m->table.entries & 0xFFFFFFFF);
        arg0 = arg0 - (uint32_t)((uint64_t)m->table.entries);
    }
    if (arg0 >= m->table.maximum) {
        sprintf(exception,
                "undefined element 0x%" PRIx32 " (max: 0x%" PRIx32 ") in table",
                arg0, m->table.maximum);
        exit(1);
    }

    arg1 = m->table.entries[arg0];
#if TRACE
    debug("       - call_indirect tidx: %d, val: 0x%x, arg1: 0x%x\n", tidx, val,
          arg1);
#endif

    if (arg1 < m->import_count) {
        // THUNK thunk_out(m, arg1);    // import/thunk call
    } else {
        Block *func = &m->functions[arg1];
        Type *ftype = func->type;

        if (m->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            exit(1);
        }

        /*
            The typesystem should have already validated the call
        if (ftype->mask != m->types[tidx].mask) {
            sprintf(exception,
                    "indirect call type mismatch (call type and function type "
                    "differ)");
            exit(1);
        }*/

        m->warduino->interpreter->setup_call(m, arg1);  // regular function call

        /*
        // Validate signatures match
       if ((int)(ftype->param_count + func->local_count) !=
           m->sp - m->fp + 1) {
           sprintf(exception,
                   "indirect call type mismatch (param counts differ)");
           exit(1);
       }



       for (uint32_t f = 0; f < ftype->param_count; f++) {
           if (ftype->params[f] != m->stack[m->fp + f].value_type) {
               sprintf(exception,
                       "indirect call type mismatch (param types differ)");
               exit(1);
           }
       }*/

#if TRACE
        debug(
            "      - tidx: %d, table idx: %d, "
            "calling function arg1: %d at: 0x%p\n",
            tidx, val, arg1, m->pc_ptr);
#endif
    }
    goto nextInstruction;

i_instr_grow_memory:
    read_LEB_32(&m->pc_ptr);  // ignore reserved
    arg0 = m->memory.pages;
    arg1 = m->stack[m->sp].value.uint32;
    m->stack[m->sp].value.uint32 = arg0;
    if (arg1 == 0) {
        goto nextInstruction;
    } else if (arg1 + arg0 > m->memory.maximum) {
        m->stack[m->sp].value.uint32 = static_cast<uint32_t>(-1);
        goto nextInstruction;
    }
    m->memory.pages += arg1;
    m->memory.bytes = (uint8_t *)arecalloc(
        m->memory.bytes, arg0 * PAGE_SIZE, m->memory.pages * PAGE_SIZE,
        1 /*sizeof(uint32_t)*/, "Module->memory.bytes", true);
    goto nextInstruction;

i_instr_br_table:
    arg0 = read_LEB_32(&m->pc_ptr);
    if (arg0 > BR_TABLE_SIZE) {
        // TODO: check this prior to runtime
        sprintf(exception, "br_table size %" PRIu32 " exceeds max %d\n", arg0,
                BR_TABLE_SIZE);
        // todo
        exit(1);
    }
    for (uint32_t i = 0; i < arg0; i++) {
        m->br_table[i] = read_LEB_32(&m->pc_ptr);
    }
    arg1 = read_LEB_32(&m->pc_ptr);
    result = m->stack[m->sp--].value.int32;

    if (result >= 0 && result < (int32_t)arg0) {
        arg1 = m->br_table[result];
    }

    m->csp -= arg1;
    // set to end for pop_block
    m->pc_ptr = m->callstack[m->csp].block->br_ptr;
#if TRACE
    debug("      - count: %d, didx: %d, to: 0x%p\n", count, didx, m->pc_ptr);
#endif
    goto nextInstruction;

i_instr_mem_load:
    flags = read_LEB_32(&m->pc_ptr);
    offset = read_LEB_32(&m->pc_ptr);
    addr = m->stack[m->sp--].value.uint32;

    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned load - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x\n",
            flags, offset, addr);
    }
    // TODO
    m->warduino->interpreter->load(m, I32 + (0x28 - opcode), addr, offset);
    goto nextInstruction;

i_instr_mem_store:
    target = &m->stack[m->sp--];
    flags = read_LEB_32(&m->pc_ptr);
    offset = read_LEB_32(&m->pc_ptr);

    addr = m->stack[m->sp--].value.uint32;

    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned store - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x, val: %s\n",
            flags, offset, addr, value_repr(target));
    }

    if (offset + addr < addr && !m->options.disable_memory_bounds) {
        m->warduino->interpreter->report_overflow(
            m, m->memory.bytes + offset + addr);
    }

    addr += offset;
    m->warduino->interpreter->store(m, I32 + (0x36 - opcode), addr, *target);
    goto nextInstruction;

i_instr_const:
    target = &m->stack[++m->sp];

    switch (opcode) {
        case I32_CONST:  // i32.const
            target->value_type = I32;
            target->value.uint32 = read_LEB_signed(&m->pc_ptr, 32);
            break;
        case I64_CONST:  // i64.const
            target->value_type = I64;
            target->value.int64 = read_LEB_signed(&m->pc_ptr, 64);
            break;
        case F32_CONST:  // f32.const
            target->value_type = F32;
            memcpy(&target->value.uint32, m->pc_ptr, 4);
            m->pc_ptr += 4;
            break;
        case F64_CONST:  // f64.const
            target->value_type = F64;
            memcpy(&target->value.uint64, m->pc_ptr, 8);
            m->pc_ptr += 8;
            break;
        default:
            goto undefinedInstruction;
    }
    goto nextInstruction;

/*
    Set_local on the stack
*/
set_local:
    arg0 = read_LEB_32(&m->pc_ptr);
    m->stack[m->fp + arg0] = m->stack[m->sp--];
    goto nextInstruction;

get_local:
    arg0 = read_LEB_32(&m->pc_ptr);
    m->stack[++m->sp] = m->stack[m->fp + arg0];
    goto nextInstruction;

tee_local:
    arg0 = read_LEB_32(&m->pc_ptr);
    m->stack[m->fp + arg0] = m->stack[m->sp];
    goto nextInstruction;

loop:
    read_LEB_32(&m->pc_ptr);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        exit(1);
    }
    m->warduino->interpreter->push_block(m, m->block_lookup[block_ptr], m->sp);
    goto nextInstruction;

block:
    read_LEB_32(&m->pc_ptr);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        exit(1);
    }
    block_itr = m->block_lookup.find(block_ptr);
    ASSERT(block_itr != m->block_lookup.end(), "could not find block");
    m->warduino->interpreter->push_block(m, block_itr->second, m->sp);
    goto nextInstruction;

    /**
     * 0x04 if
     */
i_instr_if:
    read_LEB_32(&m->pc_ptr);  // ignore block type
    block = m->block_lookup[block_ptr];

    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        exit(0);
    }
    m->warduino->interpreter->push_block(m, block, m->sp);

    arg0 = m->stack[m->sp--].value.uint32;
    if (arg0 == 0) {  // if false (I32)
        // branch to else block or after end of if
        if (block->else_ptr == nullptr) {
            // no else block, pop if block and skip end
            m->csp -= 1;
            m->pc_ptr = block->br_ptr + 1;
        } else {
            m->pc_ptr = block->else_ptr;
        }
    }
    // if true, keep going
#if TRACE
    debug("      - arg0: 0x%x jump to 0x%x, block: %s\n", arg0,
          (uint32_t)(m->pc_ptr - m->bytes), block_repr(block));
#endif
    goto nextInstruction;

i_instr_drop:
    m->sp--;
    goto nextInstruction;

/**
 * 0x05 else
 */
i_instr_else:
    block = m->callstack[m->csp].block;
    m->pc_ptr = block->br_ptr;
#if TRACE
    debug("      - of %s jump to 0x%p\n", block_repr(block), m->pc_ptr);
#endif
    goto nextInstruction;

i_instr_end:
    block = m->warduino->interpreter->pop_block(m);

    if (block == nullptr) {
        return;  // an exception (set by pop_block)
    }

    if (block->block_type == 0x00) {  // Function
        if (m->csp == -1) {
            return;  // break loop
        } else {
            // Keep going at return address
        }
    } else if (block->block_type == 0x01) {  // init_expr
        return;                              // // break loop
    } else {
    }
    goto nextInstruction;

i_instr_br_if:
    // condition
    arg0 = read_LEB_32(&m->pc_ptr);
    arg1 = m->stack[m->sp--].value.uint32;
    if (arg1) {  // if true
        m->csp -= arg0;
        // set to end for pop_block
        m->pc_ptr = m->callstack[m->csp].block->br_ptr;
    }
    goto nextInstruction;

i_instr_br:
    m->csp -= read_LEB_32(&m->pc_ptr);
    // set to end for pop_block
    m->pc_ptr = m->callstack[m->csp].block->br_ptr;
    goto nextInstruction;

call:
    fidx = read_LEB_32(&m->pc_ptr);
    if (fidx < m->import_count) {
        // primitive call
        ((Primitive)m->functions[fidx].func_ptr)(m);
    } else {
        if (m->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            exit(1);
        }
        // regular function call
        m->warduino->interpreter->setup_call(m, fidx);
    }
    goto nextInstruction;

ret:
    while (m->csp >= 0 && m->callstack[m->csp].block->block_type != 0x00) {
        m->csp--;
    }
    // Set the program count to the end of the function
    // The actual pop_block and return is handled by the end opcode.
    m->pc_ptr = m->callstack[0].block->end_ptr;
    goto nextInstruction;

i_instr_select:
    arg0 = m->stack[m->sp--].value.uint32;
    m->sp--;
    if (!arg0) {  // use a instead of b
        m->stack[m->sp] = m->stack[m->sp + 1];
    }
    goto nextInstruction;

// Binary boolean operators 32 bit
i_instr_math_u32:
    arg0_uint = m->stack[m->sp - 1].value.uint32;
    arg1_uint = m->stack[m->sp].value.uint32;
    m->sp -= 1;
    switch (opcode) {
        case I32_EQ:
            result_uint = static_cast<uint32_t>(arg0_uint == arg1_uint);
            break;
        case I32_NE:
            result_uint = static_cast<uint32_t>(arg0_uint != arg1_uint);
            break;
        case I32_LT_S:
            result_uint =
                static_cast<uint32_t>((int32_t)arg0_uint < (int32_t)arg1_uint);
            break;
        case I32_LT_U:
            result_uint = static_cast<uint32_t>(arg0_uint < arg1_uint);
            break;
        case I32_GT_S:
            result_uint =
                static_cast<uint32_t>((int32_t)arg0_uint > (int32_t)arg1_uint);
            break;
        case I32_GT_U:
            result_uint = static_cast<uint32_t>(arg0_uint > arg1_uint);
            break;
        case I32_LE_S:
            result_uint =
                static_cast<uint32_t>((int32_t)arg0_uint <= (int32_t)arg1_uint);
            break;
        case I32_LE_U:
            result_uint = static_cast<uint32_t>(arg0_uint <= arg1_uint);
            break;
        case I32_GE_S:
            result_uint =
                static_cast<uint32_t>((int32_t)arg0_uint >= (int32_t)arg1_uint);
            break;
        case I32_GE_U:
            result_uint = static_cast<uint32_t>(arg0_uint >= arg1_uint);
            break;
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }
    // m->stack[m->sp].value_type = I32; (This is already set to I32 in the
    // stack)
    m->stack[m->sp].value.uint32 = result_uint;
    goto nextInstruction;
    /// check arg0_64

i_instr_binary_i32:
    // TODO: verify if this should not be done with int32_t instead
    arg0_uint = m->stack[m->sp - 1].value.uint32;
    arg1_uint = m->stack[m->sp].value.uint32;
    m->sp -= 1;

    if (opcode >= 0x6d && opcode <= 0x70 && arg1_uint == 0) {
        sprintf(exception, "integer divide by zero");
        exit(1);
    }
    switch (opcode) {
        // case 0x6a: o = __builtin_add_overflow(a, b, &c); break;
        // // i32.add case 0x6b: o = __builtin_sub_overflow(a, b,
        // &c); break;  // i32.sub
        case I32_ADD:
            result = arg0_uint + arg1_uint;
            break;  // i32.add
        case I32_SUB:
            result = arg0_uint - arg1_uint;
            break;  // i32.sub
        case I32_MUL:
            result = arg0_uint * arg1_uint;
            break;  // i32.mul
        case I32_DIV_S:
            if (arg0_uint == 0x80000000 && arg1_uint == (uint32_t)-1) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            result = (int32_t)arg0_uint / (int32_t)arg1_uint;
            break;  // i32.div_s
        case I32_DIV_U:
            result = arg0_uint / arg1_uint;
            break;  // i32.div_u
        case I32_REM_S:
            if (arg0_uint == 0x80000000 && arg1_uint == (uint32_t)-1) {
                result = 0;
            } else {
                result = (int32_t)arg0_uint % (int32_t)arg1_uint;
            };
            break;  // i32.rem_s
        case I32_REM_U:
            result = (uint32_t)(arg0_uint % arg1_uint);
            break;  // i32.rem_u
        case I32_AND:
            result = arg0_uint & arg1_uint;
            break;  // i32.and
        case I32_OR:
            result = arg0_uint | arg1_uint;
            break;  // i32.or
        case I32_XOR:
            result = arg0_uint ^ arg1_uint;
            break;  // i32.xor
        case I32_SHL:
            result = arg0_uint << arg1_uint;
            break;  // i32.shl
        case I32_SHR_S:
            result = (int32_t)arg0_uint >>
                     arg1_uint;  // NOLINT(hicpp-signed-bitwise)
            break;               // i32.shr_s
        case I32_SHR_U:
            result = arg0_uint >> arg1_uint;
            break;  // i32.shr_u
        case I32_ROTL:
            result = rotl32(arg0_uint, arg1_uint);
            break;  // i32.rotl
        case I32_ROTR:
            result = rotr32(arg0_uint, arg1_uint);
            break;  // i32.rotr
        default:
            exit(1);
    }
    // m->stack[m->sp].value_type = I32; (This is already set to I32 in the
    // stack)
    m->stack[m->sp].value.uint32 = (uint32_t)result;
    goto nextInstruction;

math_binop_64_bool:
    arg0_64 = m->stack[m->sp - 1].value.uint64;
    arg1_64 = m->stack[m->sp].value.uint64;
    m->sp -= 1;
    switch (opcode) {
        case I64_EQ:
            result = static_cast<uint32_t>(arg0_64 == arg1_64);
            break;
        case I64_NE:
            result = static_cast<uint32_t>(arg0_64 != arg1_64);
            break;
        case I64_LT_S:
            result = static_cast<uint32_t>((int64_t)arg0_64 < (int64_t)arg1_64);
            break;
        case I64_LT_U:
            result = static_cast<uint32_t>(arg0_64 < arg1_64);
            break;
        case I64_GT_S:
            result = static_cast<uint32_t>((int64_t)arg0_64 > (int64_t)arg1_64);
            break;
        case I64_GT_U:
            result = static_cast<uint32_t>(arg0_64 > arg1_64);
            break;
        case I64_LE_S:
            result =
                static_cast<uint32_t>((int64_t)arg0_64 <= (int64_t)arg1_64);
            break;
        case I64_LE_U:
            result = static_cast<uint32_t>(arg0_64 <= arg1_64);
            break;
        case I64_GE_S:
            result =
                static_cast<uint32_t>((int64_t)arg0_64 >= (int64_t)arg1_64);
            break;
        case I64_GE_U:
            result = static_cast<uint32_t>(arg0_64 >= arg1_64);
            break;
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = result;
    goto nextInstruction;

i32_eqz:  // i32.eqz
    m->stack[m->sp].value.uint32 =
        static_cast<uint32_t>(m->stack[m->sp].value.uint32 == 0);
    goto nextInstruction;

i64_eqz:  // i64.eqz
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 =
        static_cast<uint32_t>(m->stack[m->sp].value.uint64 == 0);
    goto nextInstruction;

i_instr_binary_i64:
    arg0_int64_t = m->stack[m->sp - 1].value.uint64;
    arg1_int64_t = m->stack[m->sp].value.uint64;
    m->sp -= 1;
    if (opcode >= 0x7f && opcode <= 0x82 && arg1_int64_t == 0) {
        sprintf(exception, "integer divide by zero");
        exit(1);
    }
    switch (opcode) {
        case I64_ADD:
            result_uint64_t = arg0_int64_t + arg1_int64_t;
            break;  // i64.add
        case I64_SUB:
            result_uint64_t = arg0_int64_t - arg1_int64_t;
            break;  // i64.sub
        case I64_MUL:
            result_uint64_t = arg0_int64_t * arg1_int64_t;
            break;  // i64.mul
        case I64_DIV_S:
            if (arg0_int64_t == 0x8000000000000000 &&
                arg1_int64_t == (uint32_t)-1) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            result_uint64_t = (int64_t)arg0_int64_t / (int64_t)arg1_int64_t;
            break;  // i64.div_s
        case I64_DIV_U:
            result_uint64_t = arg0_int64_t / arg1_int64_t;
            break;  // i64.div_u
        case I64_REM_S:
            if (arg0_int64_t == 0x8000000000000000 &&
                arg1_int64_t == (uint32_t)-1) {
                result_uint64_t = 0;
            } else {
                result_uint64_t = (int64_t)arg0_int64_t % (int64_t)arg1_int64_t;
            }
            break;  // i64.rem_s
        case I64_REM_U:
            result_uint64_t = arg0_int64_t % arg1_int64_t;
            break;  // i64.rem_u
        case I64_AND:
            result_uint64_t = arg0_int64_t & arg1_int64_t;
            break;  // i64.and
        case I64_OR:
            result_uint64_t = arg0_int64_t | arg1_int64_t;
            break;  // i64.or
        case I64_XOR:
            result_uint64_t = arg0_int64_t ^ arg1_int64_t;
            break;  // i64.xor
        case I64_SHL:
            result_uint64_t = arg0_int64_t << arg1_int64_t;
            break;  // i64.shl
        case I64_SHR_S:
            result_uint64_t = ((int64_t)arg0_int64_t) >>
                              arg1_int64_t;  // NOLINT(hicpp-signed-bitwise)
            break;                           // i64.shr_s
        case I64_SHR_U:
            result_uint64_t = arg0_int64_t >> arg1_int64_t;
            break;  // i64.shr_u
        case I64_ROTL:
            result_uint64_t = rotl64(arg0_int64_t, arg1_int64_t);
            break;  // i64.rotl
        case I64_ROTR:
            result_uint64_t = rotr64(arg0_int64_t, arg1_int64_t);
            break;  // i64.rotr
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }
    m->stack[m->sp].value.uint64 = result_uint64_t;

    goto nextInstruction;

i_instr_unary_u32:
    switch (opcode) {
        case I32_EQZ:  // i32.eqz
            m->stack[m->sp].value.uint32 =
                static_cast<uint32_t>(m->stack[m->sp].value.uint32 == 0);
            break;
        case I64_EQZ:  // i64.eqz
            m->stack[m->sp].value_type = I32;
            m->stack[m->sp].value.uint32 =
                static_cast<uint32_t>(m->stack[m->sp].value.uint64 == 0);
            break;
        default:
            goto undefinedInstruction;
    }
    goto nextInstruction;

    /*
    i_instr_math_u32:
        arg0 = m->stack[m->sp - 1].value.uint32;
        arg1 = m->stack[m->sp].value.uint32;
        result;
        m->sp -= 1;
        switch (opcode) {
            case I32_EQ:
                result = static_cast<uint32_t>(arg0 == arg1);
                break;  // i32.eq
            case I32_NE:
                result = static_cast<uint32_t>(arg0 != arg1);
                break;  // i32.ne
            case I32_LT_S:
                result = static_cast<uint32_t>((int32_t)arg0 < (int32_t)arg1);
                break;  // i32.lt_s
            case I32_LT_U:
                result = static_cast<uint32_t>(arg0 < arg1);
                break;  // i32.lt_u
            case I32_GT_S:
                result = static_cast<uint32_t>((int32_t)arg0 > (int32_t)arg1);
                break;  // i32.gt_s
            case I32_GT_U:
                result = static_cast<uint32_t>(arg0 > arg1);
                break;  // i32.gt_u
            case I32_LE_S:
                result = static_cast<uint32_t>((int32_t)arg0 <= (int32_t)arg1);
                break;  // i32.le_s
            case I32_LE_U:
                result = static_cast<uint32_t>(arg0 <= arg1);
                break;  // i32.le_u
            case I32_GE_S:
                result = static_cast<uint32_t>((int32_t)arg0 >= (int32_t)arg1);
                break;  // i32.ge_s
            case I32_GE_U:
                result = static_cast<uint32_t>(arg0 >= arg1);
                break;  // i32.ge_u
            default:
                goto undefinedInstruction;
        }
        m->stack[m->sp].value_type = I32;
        m->stack[m->sp].value.uint32 = result;
        goto nextInstruction;
    */

i_instr_math_u64:
    arg0_64 = m->stack[m->sp - 1].value.uint64;
    arg1_64 = m->stack[m->sp].value.uint64;
    result;
    m->sp -= 1;
    switch (opcode) {
        case I64_EQ:
            result = static_cast<uint32_t>(arg0_64 == arg1_64);
            break;  // i64.eq
        case I64_NE:
            result = static_cast<uint32_t>(arg0_64 != arg1_64);
            break;  // i64.ne
        case I64_LT_S:
            result = static_cast<uint32_t>((int64_t)arg0_64 < (int64_t)arg1_64);
            break;  // i64.lt_s
        case I64_LT_U:
            result = static_cast<uint32_t>(arg0_64 < arg1_64);
            break;  // i64.lt_u
        case I64_GT_S:
            result = static_cast<uint32_t>((int64_t)arg0_64 > (int64_t)arg1_64);
            break;  // i64.gt_s
        case I64_GT_U:
            result = static_cast<uint32_t>(arg0_64 > arg1_64);
            break;  // i64.gt_u
        case I64_LE_S:
            result =
                static_cast<uint32_t>((int64_t)arg0_64 <= (int64_t)arg1_64);
            break;  // i64.le_s
        case I64_LE_U:
            result = static_cast<uint32_t>(arg0_64 <= arg1_64);
            break;  // i64.le_u
        case I64_GE_S:
            result =
                static_cast<uint32_t>((int64_t)arg0_64 >= (int64_t)arg1_64);
            break;  // i64.ge_s
        case I64_GE_U:
            result = static_cast<uint32_t>(arg0_64 >= arg1_64);
            break;  // i64.ge_u
        default:
            goto undefinedInstruction;
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = result;
    goto nextInstruction;

i_instr_math_f32:
    arg0_f = m->stack[m->sp - 1].value.f32;
    arg1_f = m->stack[m->sp].value.f32;
    m->sp -= 1;
    switch (opcode) {
        case F32_EQ:
            result = static_cast<uint32_t>(arg0_f == arg1_f);
            break;  // f32.eq
        case F32_NE:
            result = static_cast<uint32_t>(arg0_f != arg1_f);
            break;  // f32.ne
        case F32_LT:
            result = static_cast<uint32_t>(arg0_f < arg1_f);
            break;  // f32.lt
        case F32_GT:
            result = static_cast<uint32_t>(arg0_f > arg1_f);
            break;  // f32.gt
        case F32_LE:
            result = static_cast<uint32_t>(arg0_f <= arg1_f);
            break;  // f32.le
        case F32_GE:
            result = static_cast<uint32_t>(arg0_f >= arg1_f);
            break;  // f32.ge
        default:
            goto undefinedInstruction;
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = result;
    goto nextInstruction;

i_instr_math_f64:
    arg0_d = m->stack[m->sp - 1].value.f64;
    arg1_d = m->stack[m->sp].value.f64;
    m->sp -= 1;
    switch (opcode) {
        case F64_EQ:
            result = static_cast<uint32_t>(arg0_d == arg1_d);
            break;  // f64.eq
        case F64_NE:
            result = static_cast<uint32_t>(arg0_d != arg1_d);
            break;  // f64.ne
        case F64_LT:
            result = static_cast<uint32_t>(arg0_d < arg1_d);
            break;  // f64.lt
        case F64_GT:
            result = static_cast<uint32_t>(arg0_d > arg1_d);
            break;  // f64.gt
        case F64_LE:
            result = static_cast<uint32_t>(arg0_d <= arg1_d);
            break;  // f64.le
        case F64_GE:
            result = static_cast<uint32_t>(arg0_d >= arg1_d);
            break;  // f64.ge
        default:
            goto undefinedInstruction;
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = result;
    goto nextInstruction;

i_instr_unary_i32:
    arg0 = m->stack[m->sp].value.uint32;
    switch (opcode) {
        case I32_CLZ:
            result = arg0 == 0 ? 32 : __builtin_clz(arg0);
            break;  // i32.clz
        case I32_CTZ:
            result = arg0 == 0 ? 32 : __builtin_ctz(arg0);
            break;  // i32.ctz
        case I32_POPCNT:
            result = __builtin_popcount(arg0);
            break;  // i32.popcnt
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }
    m->stack[m->sp].value.uint32 = result;
    goto nextInstruction;

i_instr_unary_i64:
    arg0_64 = m->stack[m->sp].value.uint64;
    switch (opcode) {
        case I64_CLZ:
            result_uint64_t = arg0_64 == 0 ? 64 : __builtin_clzll(arg0_64);
            break;  // i64.clz
        case I64_CTZ:
            result_uint64_t = arg0_64 == 0 ? 64 : __builtin_ctzll(arg0_64);
            break;  // i64.ctz
        case I64_POPCNT:
            result_uint64_t = __builtin_popcountll(arg0_64);
            break;  // i64.popcnt
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }
    m->stack[m->sp].value.uint64 = result_uint64_t;
    goto nextInstruction;

i_instr_unary_floating:
    switch (opcode) {
        // unary f32
        case F32_ABS:
            m->stack[m->sp].value.f32 = fabs(m->stack[m->sp].value.f32);
            break;  // f32.abs
        case F32_NEG:
            m->stack[m->sp].value.f32 = -m->stack[m->sp].value.f32;
            break;  // f32.neg
        case F32_CEIL:
            m->stack[m->sp].value.f32 = ceil(m->stack[m->sp].value.f32);
            break;  // f32.ceil
        case F32_FLOOR:
            m->stack[m->sp].value.f32 = floor(m->stack[m->sp].value.f32);
            break;  // f32.floor
        case F32_TRUNC:
            m->stack[m->sp].value.f32 = trunc(m->stack[m->sp].value.f32);
            break;  // f32.trunc
        case F32_NEAREST:
            m->stack[m->sp].value.f32 = rint(m->stack[m->sp].value.f32);
            break;  // f32.nearest
        case F32_SQRT:
            m->stack[m->sp].value.f32 = sqrt(m->stack[m->sp].value.f32);
            break;  // f32.sqrt
            // unary f64
        case F64_ABS:
            m->stack[m->sp].value.f64 = fabs(m->stack[m->sp].value.f64);
            break;  // f64.abs
        case F64_NEG:
            m->stack[m->sp].value.f64 = -m->stack[m->sp].value.f64;
            break;  // f64.neg
        case F64_CEIL:
            m->stack[m->sp].value.f64 = ceil(m->stack[m->sp].value.f64);
            break;  // f64.ceil
        case F64_FLOOR:
            m->stack[m->sp].value.f64 = floor(m->stack[m->sp].value.f64);
            break;  // f64.floor
        case F64_TRUNC:
            m->stack[m->sp].value.f64 = trunc(m->stack[m->sp].value.f64);
            break;  // f64.trunc
        case F64_NEAREST:
            m->stack[m->sp].value.f64 = rint(m->stack[m->sp].value.f64);
            break;  // f64.nearest
        case F64_SQRT:
            m->stack[m->sp].value.f64 = sqrt(m->stack[m->sp].value.f64);
            break;  // f64.sqrt
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }

    goto nextInstruction;

i_instr_binary_f32:
    arg0_f = m->stack[m->sp - 1].value.f32;
    arg1_f = m->stack[m->sp].value.f32;
    m->sp -= 1;
    switch (opcode) {
        case F32_ADD:
            fresult = arg0_f + arg1_f;
            break;  // f32.add
        case F32_SUB:
            fresult = arg0_f - arg1_f;
            break;  // f32.sub
        case F32_MUL:
            fresult = arg0_f * arg1_f;
            break;  // f32.mul
        case F32_DIV:
            fresult = arg0_f / arg1_f;
            break;  // f32.div
        case F32_MIN:
            fresult = (float)wa_fmin(arg0_f, arg1_f);
            break;  // f32.min
        case F32_MAX:
            fresult = (float)wa_fmax(arg0_f, arg1_f);
            break;  // f32.max
        case F32_COPYSIGN:
            fresult = std::signbit(arg1_f) ? -fabs(arg0_f) : fabs(arg0_f);
            break;  // f32.copysign
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }
    m->stack[m->sp].value.f32 = fresult;
    goto nextInstruction;

i_instr_binary_f64:
    arg0_d = m->stack[m->sp - 1].value.f64;
    arg1_d = m->stack[m->sp].value.f64;
    m->sp -= 1;
    switch (opcode) {
        case F64_ADD:
            result_d = arg0_d + arg1_d;
            break;  // f64.add
        case F64_SUB:
            result_d = arg0_d - arg1_d;
            break;  // f64.sub
        case F64_MUL:
            result_d = arg0_d * arg1_d;
            break;  // f64.mul
        case F64_DIV:
            result_d = arg0_d / arg1_d;
            break;  // f64.div
        case F64_MIN:
            result_d = wa_fmin(arg0_d, arg1_d);
            break;  // f64.min
        case F64_MAX:
            result_d = wa_fmax(arg0_d, arg1_d);
            break;  // f64.max
        case F64_COPYSIGN:
            result_d = std::signbit(arg1_d) ? -fabs(arg0_d) : fabs(arg0_d);
            break;  // f64.copysign
        default:
            printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
            exit(1);
    }
    m->stack[m->sp].value.f64 = result_d;
    goto nextInstruction;

/**
 *
 */
i_instr_conversion:
    switch (opcode) {
        case I32_WRAP_I64:
            m->stack[m->sp].value.uint64 &= 0x00000000ffffffff;
            m->stack[m->sp].value_type = I32;
            break;  // i32.wrap/i64
        case I32_TRUNC_F32_S:
            if (std::isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f32 >= INT32_MAX ||
                       m->stack[m->sp].value.f32 < INT32_MIN) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.int32 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_s/f32
        case I32_TRUNC_F32_U:
            if (std::isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f32 >= UINT32_MAX ||
                       m->stack[m->sp].value.f32 <= -1) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.uint32 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_u/f32
        case I32_TRUNC_F64_S:
            if (std::isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f64 > INT32_MAX ||
                       m->stack[m->sp].value.f64 < INT32_MIN) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.int32 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_s/f64
        case I32_TRUNC_F64_U:
            if (std::isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f64 > UINT32_MAX ||
                       m->stack[m->sp].value.f64 <= -1) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.uint32 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I32;
            break;  // i32.trunc_u/f64
        case I64_EXTEND_I32_S:
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.uint32;
            sext_32_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.extend_s/i32
        case I64_EXTEND_I32_U:
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.uint32;
            m->stack[m->sp].value_type = I64;
            break;  // i64.extend_u/i32
        case I64_TRUNC_F32_S:
            if (std::isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f32 >= INT64_MAX ||
                       m->stack[m->sp].value.f32 < INT64_MIN) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.int64 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_s/f32
        case I64_TRUNC_F32_U:
            if (std::isnan(m->stack[m->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f32 >= UINT64_MAX ||
                       m->stack[m->sp].value.f32 <= -1) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_u/f32
        case I64_TRUNC_F64_S:
            if (std::isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f64 >= INT64_MAX ||
                       m->stack[m->sp].value.f64 < INT64_MIN) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.int64 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_s/f64
        case I64_TRUNC_F64_U:
            if (std::isnan(m->stack[m->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                exit(1);
            } else if (m->stack[m->sp].value.f64 >= UINT64_MAX ||
                       m->stack[m->sp].value.f64 <= -1) {
                sprintf(exception, "integer overflow");
                exit(1);
            }
            m->stack[m->sp].value.uint64 = m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = I64;
            break;  // i64.trunc_u/f64
        case F32_CONVERT_I32_S:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.int32;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_s/i32
        case F32_CONVERT_I32_U:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.uint32;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_u/i32
        case F32_CONVERT_I64_S:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.int64;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_s/i64
        case F32_CONVERT_I64_U:
            m->stack[m->sp].value.f32 = m->stack[m->sp].value.uint64;
            m->stack[m->sp].value_type = F32;
            break;  // f32.convert_u/i64
        case F32_DEMOTE_F64:
            m->stack[m->sp].value.f32 = (float)m->stack[m->sp].value.f64;
            m->stack[m->sp].value_type = F32;
            break;  // f32.demote/f64
        case F64_CONVERT_I32_S:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.int32;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_s/i32
        case F64_CONVERT_I32_U:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.uint32;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_u/i32
        case F64_CONVERT_I64_S:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.int64;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_s/i64
        case F64_CONVERT_I64_U:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.uint64;
            m->stack[m->sp].value_type = F64;
            break;  // f64.convert_u/i64
        case F64_PROMOTE_F32:
            m->stack[m->sp].value.f64 = m->stack[m->sp].value.f32;
            m->stack[m->sp].value_type = F64;
            break;  // f64.promote/f32

            // reinterpretations
        case I32_REINTERPRET_F32:
            m->stack[m->sp].value_type = I32;
            break;  // i32.reinterpret/f32
        case I64_REINTERPRET_F64:
            m->stack[m->sp].value_type = I64;
            break;                 // i64.reinterpret/f64
        case F32_REINTERPRET_I32:  // memmove(&m->stack[m->sp].value.f32,
            // &m->stack[m->sp].value.uint32, 4);
            m->stack[m->sp].value_type = F32;
            break;  // f32.reinterpret/i32
        case F64_REINTERPRET_I64:
            m->stack[m->sp].value_type = F64;
            break;  // f64.reinterpret/i64
        default:
            exit(1);
    }

    goto nextInstruction;

    // Administrative instructions

/**
 * 0x23 get_global
 */
i_instr_get_global:
    arg0 = read_LEB_32(&m->pc_ptr);
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg0, value_repr(&m->globals[arg0]));
#endif
    m->stack[++m->sp] = m->globals[arg0];
    goto nextInstruction;

nextInstruction:
    /*
    BIG DUMP OF OLD CODE
    */

    if (m->warduino->program_state == WARDUINOstep) {
        m->warduino->debugger->pauseRuntime(m);
    }

    while (m->warduino->program_state != WARDUINOinit &&
           m->warduino->debugger->checkDebugMessages(
               m, &m->warduino->program_state)) {
    }
    fflush(stdout);
    //        esp_task_wdt_reset();

    // Resolve 1 callback event if queue is not empty and VM not paused, and
    // no event currently resolving
    CallbackHandler::resolve_event();

    // Sleep interpret loop if 'paused' or 'waiting drone'
    if (m->warduino->program_state == WARDUINOpause ||
        m->warduino->program_state == PROXYhalt) {
        // wait until new debug messages arrive
        if (m->warduino->program_state == WARDUINOpause) {
            warduino::unique_lock lock(
                m->warduino->debugger->messageQueueMutex);
            m->warduino->debugger->messageQueueConditionVariable.wait(
                lock, [m] { return m->warduino->debugger->freshMessages; });
        }
        goto nextInstruction;
    }

    // Program state is not paused

    // If BP and not the one we just unpaused
    if (m->warduino->debugger->isBreakpoint(m->pc_ptr) &&
        m->warduino->debugger->skipBreakpoint != m->pc_ptr &&
        m->warduino->program_state != PROXYrun) {
        m->warduino->debugger->pauseRuntime(m);
        m->warduino->debugger->notifyBreakpoint(m, m->pc_ptr);
        goto nextInstruction;
    }
    m->warduino->debugger->skipBreakpoint = nullptr;

    // Take snapshot before executing an instruction
    m->warduino->debugger->sendAsyncSnapshots(m);

    dbg_dump_stack(m);
    dbg_trace(" <<<>><<>> PC: %p OPCODE: <%s> in %s\n", block_ptr,
              opcode_repr(opcode),
              m->pc_ptr > m->bytes && m->pc_ptr < m->bytes + m->byte_count
                  ? "module"
                  : "patch");

    // END DUMP OF OLD CODE
    opcode = *m->pc_ptr;
    block_ptr = m->pc_ptr;
    m->pc_ptr += 1;
    goto *jumpTable[opcode];

undefinedInstruction:
    printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
    exit(0);

// initialize of the jump table should only happen once
initJumpTable:
    jumpTable[NOP] = &&nextInstruction;

    // set the jump table for the instructions
    jumpTable[I32_CONST] = &&i_instr_const;
    jumpTable[I64_CONST] = &&i_instr_const;
    jumpTable[F32_CONST] = &&i_instr_const;
    jumpTable[F64_CONST] = &&i_instr_const;

    jumpTable[SET_LOCAL] = &&set_local;
    jumpTable[GET_LOCAL] = &&get_local;
    jumpTable[LOOP] = &&loop;
    jumpTable[BLOCK_I] = &&block;
    jumpTable[END] = &&i_instr_end;
    jumpTable[CALL] = &&call;
    jumpTable[BR_IF] = &&i_instr_br_if;
    jumpTable[BR] = &&i_instr_br;
    jumpTable[RETURN] = &&ret;
    jumpTable[TEE_LOCAL] = &&tee_local;
    jumpTable[SELECT] = &&i_instr_select;

    jumpTable[BR_TABLE] = &&i_instr_br_table;

    // boolean instructions for 32 bit
    jumpTable[I32_EQ] = &&i_instr_math_u32;
    jumpTable[I32_NE] = &&i_instr_math_u32;
    jumpTable[I32_LT_S] = &&i_instr_math_u32;
    jumpTable[I32_LT_U] = &&i_instr_math_u32;
    jumpTable[I32_GT_S] = &&i_instr_math_u32;
    jumpTable[I32_GT_U] = &&i_instr_math_u32;
    jumpTable[I32_LE_S] = &&i_instr_math_u32;
    jumpTable[I32_LE_U] = &&i_instr_math_u32;
    jumpTable[I32_GE_S] = &&i_instr_math_u32;
    jumpTable[I32_GE_U] = &&i_instr_math_u32;
    // boolean instructions for 64 bit
    jumpTable[I64_EQ] = &&math_binop_64_bool;
    jumpTable[I64_NE] = &&math_binop_64_bool;
    jumpTable[I64_LT_S] = &&math_binop_64_bool;
    jumpTable[I64_LT_U] = &&math_binop_64_bool;
    jumpTable[I64_GT_S] = &&math_binop_64_bool;
    jumpTable[I64_GT_U] = &&math_binop_64_bool;
    jumpTable[I64_LE_S] = &&math_binop_64_bool;
    jumpTable[I64_LE_U] = &&math_binop_64_bool;
    jumpTable[I64_GE_S] = &&math_binop_64_bool;
    jumpTable[I64_GE_U] = &&math_binop_64_bool;
    // binop instructions for 32 bit
    jumpTable[I32_ADD] = &&i_instr_binary_i32;
    jumpTable[I32_SUB] = &&i_instr_binary_i32;
    jumpTable[I32_MUL] = &&i_instr_binary_i32;
    jumpTable[I32_DIV_S] = &&i_instr_binary_i32;
    jumpTable[I32_DIV_U] = &&i_instr_binary_i32;
    jumpTable[I32_REM_S] = &&i_instr_binary_i32;
    jumpTable[I32_REM_U] = &&i_instr_binary_i32;
    jumpTable[I32_AND] = &&i_instr_binary_i32;
    jumpTable[I32_OR] = &&i_instr_binary_i32;
    jumpTable[I32_XOR] = &&i_instr_binary_i32;
    jumpTable[I32_SHL] = &&i_instr_binary_i32;
    jumpTable[I32_SHR_S] = &&i_instr_binary_i32;
    jumpTable[I32_SHR_U] = &&i_instr_binary_i32;
    jumpTable[I32_ROTL] = &&i_instr_binary_i32;
    jumpTable[I32_ROTR] = &&i_instr_binary_i32;

    jumpTable[F32_ADD] = &&i_instr_binary_f32;
    jumpTable[F32_SUB] = &&i_instr_binary_f32;
    jumpTable[F32_MUL] = &&i_instr_binary_f32;
    jumpTable[F32_DIV] = &&i_instr_binary_f32;
    jumpTable[F32_MIN] = &&i_instr_binary_f32;
    jumpTable[F32_MAX] = &&i_instr_binary_f32;
    jumpTable[F32_COPYSIGN] = &&i_instr_binary_f32;

    jumpTable[I64_ADD] = &&i_instr_binary_i64;
    jumpTable[I64_SUB] = &&i_instr_binary_i64;
    jumpTable[I64_MUL] = &&i_instr_binary_i64;
    jumpTable[I64_DIV_S] = &&i_instr_binary_i64;
    jumpTable[I64_DIV_U] = &&i_instr_binary_i64;
    jumpTable[I64_REM_S] = &&i_instr_binary_i64;
    jumpTable[I64_REM_U] = &&i_instr_binary_i64;
    jumpTable[I64_AND] = &&i_instr_binary_i64;
    jumpTable[I64_OR] = &&i_instr_binary_i64;
    jumpTable[I64_XOR] = &&i_instr_binary_i64;
    jumpTable[I64_SHL] = &&i_instr_binary_i64;
    jumpTable[I64_SHR_S] = &&i_instr_binary_i64;
    jumpTable[I64_SHR_U] = &&i_instr_binary_i64;
    jumpTable[I64_ROTL] = &&i_instr_binary_i64;
    jumpTable[I64_ROTR] = &&i_instr_binary_i64;

    jumpTable[F32_ABS] = &&i_instr_unary_floating;
    jumpTable[F32_NEG] = &&i_instr_unary_floating;
    jumpTable[F32_CEIL] = &&i_instr_unary_floating;
    jumpTable[F32_FLOOR] = &&i_instr_unary_floating;
    jumpTable[F32_TRUNC] = &&i_instr_unary_floating;
    jumpTable[F32_NEAREST] = &&i_instr_unary_floating;
    jumpTable[F32_SQRT] = &&i_instr_unary_floating;
    jumpTable[F64_ABS] = &&i_instr_unary_floating;
    jumpTable[F64_NEG] = &&i_instr_unary_floating;
    jumpTable[F64_CEIL] = &&i_instr_unary_floating;
    jumpTable[F64_FLOOR] = &&i_instr_unary_floating;
    jumpTable[F64_TRUNC] = &&i_instr_unary_floating;
    jumpTable[F64_NEAREST] = &&i_instr_unary_floating;
    jumpTable[F64_SQRT] = &&i_instr_unary_floating;

    jumpTable[F64_ADD] = &&i_instr_binary_f64;
    jumpTable[F64_SUB] = &&i_instr_binary_f64;
    jumpTable[F64_MUL] = &&i_instr_binary_f64;
    jumpTable[F64_DIV] = &&i_instr_binary_f64;
    jumpTable[F64_MIN] = &&i_instr_binary_f64;
    jumpTable[F64_MAX] = &&i_instr_binary_f64;
    jumpTable[F64_COPYSIGN] = &&i_instr_binary_f64;

    jumpTable[I32_WRAP_I64] = &&i_instr_conversion;
    jumpTable[I32_TRUNC_F32_S] = &&i_instr_conversion;
    jumpTable[I32_TRUNC_F32_U] = &&i_instr_conversion;
    jumpTable[I32_TRUNC_F64_S] = &&i_instr_conversion;
    jumpTable[I32_TRUNC_F64_U] = &&i_instr_conversion;
    jumpTable[I64_EXTEND_I32_S] = &&i_instr_conversion;
    jumpTable[I64_EXTEND_I32_U] = &&i_instr_conversion;
    jumpTable[I64_TRUNC_F32_S] = &&i_instr_conversion;
    jumpTable[I64_TRUNC_F32_U] = &&i_instr_conversion;
    jumpTable[I64_TRUNC_F64_S] = &&i_instr_conversion;
    jumpTable[I64_TRUNC_F64_U] = &&i_instr_conversion;
    jumpTable[F32_CONVERT_I32_S] = &&i_instr_conversion;
    jumpTable[F32_CONVERT_I32_U] = &&i_instr_conversion;
    jumpTable[F32_CONVERT_I64_S] = &&i_instr_conversion;
    jumpTable[F32_CONVERT_I64_U] = &&i_instr_conversion;
    jumpTable[F32_DEMOTE_F64] = &&i_instr_conversion;
    jumpTable[F64_CONVERT_I32_S] = &&i_instr_conversion;
    jumpTable[F64_CONVERT_I32_U] = &&i_instr_conversion;
    jumpTable[F64_CONVERT_I64_S] = &&i_instr_conversion;
    jumpTable[F64_CONVERT_I64_U] = &&i_instr_conversion;
    jumpTable[F64_PROMOTE_F32] = &&i_instr_conversion;
    jumpTable[I32_REINTERPRET_F32] = &&i_instr_conversion;
    jumpTable[I64_REINTERPRET_F64] = &&i_instr_conversion;
    jumpTable[F32_REINTERPRET_I32] = &&i_instr_conversion;
    jumpTable[F64_REINTERPRET_I64] = &&i_instr_conversion;

    // math
    /*  jumpTable[I32_EQ] = &&i_instr_math_u32;
      jumpTable[I32_NE] = &&i_instr_math_u32;
      jumpTable[I32_LT_S] = &&i_instr_math_u32;
      jumpTable[I32_LT_U] = &&i_instr_math_u32;
      jumpTable[I32_GT_S] = &&i_instr_math_u32;
      jumpTable[I32_GT_U] = &&i_instr_math_u32;
      jumpTable[I32_LE_S] = &&i_instr_math_u32;
      jumpTable[I32_LE_U] = &&i_instr_math_u32;
      jumpTable[I32_GE_S] = &&i_instr_math_u32;
      jumpTable[I32_GE_U] = &&i_instr_math_u32;*/

    jumpTable[I64_EQ] = &&i_instr_math_u64;
    jumpTable[I64_NE] = &&i_instr_math_u64;
    jumpTable[I64_LT_S] = &&i_instr_math_u64;
    jumpTable[I64_LT_U] = &&i_instr_math_u64;
    jumpTable[I64_GT_S] = &&i_instr_math_u64;
    jumpTable[I64_GT_U] = &&i_instr_math_u64;
    jumpTable[I64_LE_S] = &&i_instr_math_u64;
    jumpTable[I64_LE_U] = &&i_instr_math_u64;
    jumpTable[I64_GE_S] = &&i_instr_math_u64;
    jumpTable[I64_GE_U] = &&i_instr_math_u64;

    jumpTable[F64_EQ] = &&i_instr_math_f64;
    jumpTable[F64_NE] = &&i_instr_math_f64;
    jumpTable[F64_LT] = &&i_instr_math_f64;
    jumpTable[F64_GT] = &&i_instr_math_f64;
    jumpTable[F64_LE] = &&i_instr_math_f64;
    jumpTable[F64_GE] = &&i_instr_math_f64;

    jumpTable[I64_CLZ] = &&i_instr_unary_i64;
    jumpTable[I64_CTZ] = &&i_instr_unary_i64;
    jumpTable[I64_POPCNT] = &&i_instr_unary_i64;

    jumpTable[I32_EQZ] = &&i32_eqz;
    jumpTable[I64_EQZ] = &&i64_eqz;

    jumpTable[F32_EQ] = &&i_instr_math_f32;
    jumpTable[F32_NE] = &&i_instr_math_f32;
    jumpTable[F32_LT] = &&i_instr_math_f32;
    jumpTable[F32_GT] = &&i_instr_math_f32;
    jumpTable[F32_LE] = &&i_instr_math_f32;
    jumpTable[F32_GE] = &&i_instr_math_f32;

    jumpTable[I32_CLZ] = &&i_instr_unary_i32;
    jumpTable[I32_CTZ] = &&i_instr_unary_i32;
    jumpTable[I32_POPCNT] = &&i_instr_unary_i32;

    jumpTable[CALL_INDIRECT] = &&i_instr_call_indirect;
    jumpTable[GROW_MEMORY] = &&i_instr_grow_memory;

    jumpTable[SET_GLOBAL] = &&i_instr_set_global;
    jumpTable[GET_GLOBAL] = &&i_instr_get_global;

    jumpTable[IF] = &&i_instr_if;
    jumpTable[ELSE] = &&i_instr_else;

    jumpTable[DROP] = &&i_instr_drop;

    int i = MEMORY_LOAD_START;
    while (i <= MEMORY_LOAD_END) {
        jumpTable[i] = &&i_instr_mem_load;
        i++;
    }

    jumpTable[MEMORY_LOAD_END] = &&i_instr_mem_load;

    i = MEMORY_STORE_START;
    while (i <= MEMORY_STORE_END) {
        jumpTable[i] = &&i_instr_mem_store;
        i++;
    }
    jumpTable[MEMORY_STORE_END] = &&i_instr_mem_store;

    goto nextInstruction;
}

bool Interpreter::interpret(Module *m, bool waiting) {
    uint8_t *block_ptr;
    uint8_t opcode;

    // keep track of occurring errors
    bool success = true;

    // set to true when finishes successfully
    bool program_done = false;

    while ((!program_done && success) || waiting) {
        if (m->warduino->program_state == WARDUINOstep) {
            m->warduino->debugger->pauseRuntime(m);
        }

        while (m->warduino->program_state != WARDUINOinit &&
               m->warduino->debugger->checkDebugMessages(
                   m, &m->warduino->program_state)) {
        }
        fflush(stdout);
        //        esp_task_wdt_reset();

        // Resolve 1 callback event if queue is not empty and VM not paused,
        // and no event currently resolving
        CallbackHandler::resolve_event();

        // Sleep interpret loop if 'paused' or 'waiting drone'
        if (m->warduino->program_state == WARDUINOpause ||
            m->warduino->program_state == PROXYhalt) {
            // wait until new debug messages arrive
            if (m->warduino->program_state == WARDUINOpause) {
                warduino::unique_lock lock(
                    m->warduino->debugger->messageQueueMutex);
                m->warduino->debugger->messageQueueConditionVariable.wait(
                    lock, [m] { return m->warduino->debugger->freshMessages; });
            }
            continue;
        }

        // Program state is not paused

        // If BP and not the one we just unpaused
        if (m->warduino->debugger->isBreakpoint(m->pc_ptr) &&
            m->warduino->debugger->skipBreakpoint != m->pc_ptr &&
            m->warduino->program_state != PROXYrun) {
            m->warduino->debugger->pauseRuntime(m);
            m->warduino->debugger->notifyBreakpoint(m, m->pc_ptr);
            continue;
        }
        m->warduino->debugger->skipBreakpoint = nullptr;

        // Take snapshot before executing an instruction
        m->warduino->debugger->sendAsyncSnapshots(m);

        opcode = *m->pc_ptr;
        block_ptr = m->pc_ptr;
        m->pc_ptr += 1;

        dbg_dump_stack(m);
        dbg_trace(" PC: %p OPCODE: <%s> in %s\n", block_ptr,
                  opcode_repr(opcode),
                  m->pc_ptr > m->bytes && m->pc_ptr < m->bytes + m->byte_count
                      ? "module"
                      : "patch");

        switch (opcode) {
            //
            // Control flow operators
            //
            case UNREACHABLE:
                sprintf(exception, "%s", "unreachable");
                success &= false;
            case NOP:
                continue;
            case BLOCK_I:
                success &= i_instr_block(m, block_ptr);
                continue;
            case LOOP:
                success &= i_instr_loop(m, block_ptr);
                continue;
            case IF:
                success &= i_instr_if(m, block_ptr);
                continue;
            case ELSE:  // else
                success &= i_instr_else(m);
                continue;
            case END:  // end
                success &= i_instr_end(m, &program_done);
                continue;
            case 0x0c:  // br
                success &= i_instr_br(m);
                continue;
            case 0x0d:  // br_if
                success &= i_instr_br_if(m);
                continue;
            case 0x0e:  // br_table
                success &= i_instr_br_table(m);
                continue;
            case 0x0f:  // return
                success &= i_instr_return(m);
                continue;

                //
                // Call operators
                //
            case 0x10: {  // call
                success &= i_instr_call(m);
                continue;
            }
            case 0x11: {  // call_indirect
                success &= i_instr_call_indirect(m);
                continue;
            }
                //
                // Parametric operators
                //
            case 0x1a:  // drop
                success &= i_instr_drop(m);
                continue;
            case 0x1b:  // select
                success &= i_instr_select(m);
                continue;

                //
                // Variable access
                //
            case 0x20:  // get_local
                success &= i_instr_get_local(m);
                continue;
            case 0x21:  // set_local
                success &= i_instr_set_local(m);
                continue;
            case 0x22:  // tee_local
                success &= i_instr_tee_local(m);
                continue;
            case 0x23:  // get_global
                success &= i_instr_get_global(m);
                continue;
            case 0x24:  // set_global
                success &= i_instr_set_global(m);
                continue;

                //
                // Memory-related operators
                //
            case 0x3f:  // current_memory
                success &= i_instr_current_memory(m);
                continue;
            case 0x40:  // grow_memory
                success &= i_instr_grow_memory(m);
                continue;
                // Memory load operators
            case 0x28 ... 0x35:
                success &= i_instr_mem_load(m, opcode);
                continue;
                // Memory store operators
            case 0x36 ... 0x3e:
                success &= i_instr_mem_store(m, opcode);
                continue;

                //
                // Constants
                //
            case 0x41 ... 0x44:  // i32.const
                success &= i_instr_const(m, opcode);
                continue;

                //
                // Comparison operators
                //

                // unary
            case 0x45:  // i32.eqz
            case 0x50:  // i64.eqz
                success &= i_instr_unary_u32(m, opcode);
                continue;

                // i32 binary
            case 0x46 ... 0x4f:
                success &= i_instr_math_u32(m, opcode);
                continue;
            case 0x51 ... 0x5a:
                success &= i_instr_math_u64(m, opcode);
                continue;
            case 0x5b ... 0x60:
                success &= i_instr_math_f32(m, opcode);
                continue;
            case 0x61 ... 0x66:
                success &= i_instr_math_f64(m, opcode);
                continue;

                //
                // Numeric operators
                //

                // unary i32
            case 0x67 ... 0x69:
                success &= i_instr_unary_i32(m, opcode);
                continue;

                // unary i64
            case 0x79 ... 0x7b:
                success &= i_instr_unary_i64(m, opcode);
                continue;

            case 0x8b ... 0x91:  // unary f32
            case 0x99 ... 0x9f:  // unary f64
                success &= i_instr_unary_floating(m, opcode);
                continue;

                // i32 binary
            case 0x6a ... 0x78:
                success &= i_instr_binary_i32(m, opcode);
                continue;

                // i64 binary
            case 0x7c ... 0x8a:
                success &= i_instr_binary_i64(m, opcode);
                continue;

                // f32 binary
            case 0x92 ... 0x98:
                success &= i_instr_binary_f32(m, opcode);
                continue;

                // f64 binary
            case 0xa0 ... 0xa6:
                success &= i_instr_binary_f64(m, opcode);
                continue;

                // conversion operations
            case 0xa7 ... 0xbb:
                success &= i_instr_conversion(m, opcode);
                continue;

                // callback operations
            case 0xe0 ... 0xe3:
                success &= i_instr_callback(m, opcode);
                continue;
            default:
                printf("unrecognized opcode 0x%x \n", opcode);

                sprintf(exception, "unrecognized opcode 0x%x \n", opcode);
                if (m->options.return_exception) {
                    m->exception = strdup(exception);
                }
                return false;
        }
    }

    if (m->warduino->program_state == PROXYrun) {
        dbg_info("Trap was thrown during proxy call.\n");
        RFC *rfc = m->warduino->debugger->topProxyCall();
        rfc->success = false;
        rfc->exception = strdup(exception);
        rfc->exception_size = strlen(exception);
        m->warduino->debugger->sendProxyCallResult(m);
    }

    // Resolve all unhandled callback events
    while (CallbackHandler::resolving_event &&
           CallbackHandler::resolve_event());

    dbg_trace("Interpretation ended %s with status %s\n",
              program_done ? "expectedly" : "unexpectedly",
              success ? "ok" : "error");
    if (!success && m->options.return_exception) {
        m->exception = strdup(exception);
    } else if (!success) {
        FATAL("%s\n", exception);
    }

    return success;
}

void Interpreter::report_overflow(Module *m, uint8_t *maddr) {
    dbg_warn("memory start: %p, memory end: %p, maddr: %p\n", m->memory.bytes,
             m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE, maddr);
    sprintf(exception, "out of bounds memory access");
}
