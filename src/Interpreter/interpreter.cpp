#include "interpreter.h"

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"

// Size of memory load.
// This starts with the first memory load operator at opcode 0x28
uint32_t LOAD_SIZE[] = {4, 8, 4, 8, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4,  // loads
                        4, 8, 4, 8, 1, 2, 1, 2, 4};                // stores

//
// Stack machine (byte code related functions)
//
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
    Block *func = m->functions[fidx];
    Type *type = func->type;

    // Push current frame on the call stack
    push_block(m, func, m->sp - type->param_count);

#if TRACE
    dbg_warn("  >> fn0x%x(%d) %s(", fidx, fidx,
             func->export_name
                 ? func->export_name
                 : "") for (int p = ((int)type->param_count) - 1; p >= 0; p--) {
        dbg_warn("%s%s", value_repr(&m->stack[m->sp - p]), p ? " " : "");
    }
    dbg_warn("), %d locals, %d results\n", func->local_count,
             type->result_count);
#endif

    // Push locals (dropping extras)
    m->fp = m->sp - ((int)type->param_count) + 1;
    // TODO: validate arguments vs formal params

    // Push function locals
    for (uint32_t lidx = 0; lidx < func->local_count; lidx++) {
        m->sp += 1;
        m->stack[m->sp].value_type = func->local_value_type[lidx];
        m->stack[m->sp].value.uint64 = 0;  // Initialize whole union to 0
    }

    // Set program counter to start of function
    m->pc_ptr = func->start_ptr;
}

// performs proxy calls to an MCU
bool proxy_call(Module *m, uint32_t fidx) {
    dbg_info("Remote Function Call %d\n", fidx);
    ProxySupervisor *supervisor = m->warduino->debugger->supervisor;
    RFC *rfc;
    Type *type = m->functions[fidx]->type;
    if (type->param_count > 0) {
        m->sp -= type->param_count;
        StackValue *args = &m->stack[m->sp + 1];
        rfc = new RFC(fidx, type, args);
    } else {
        rfc = new RFC(fidx, type);
    }

    if (!supervisor->call(rfc)) {
        dbg_info(": FAILED TO SEND\n", fidx);
        return false;
    }

    if (!rfc->success) {
        // TODO exception bugger might be too small and msg not null terminated?
        memcpy(&exception, rfc->exception, strlen(rfc->exception));
        return false;
    }

    if (rfc->type->result_count > 0) {
        m->stack[++m->sp] = *rfc->result;
    }
    return true;
}

/*
 * WebAssembly Instructions
 *
 * i_instr_**** functions
 *
 * Functions here are called from a dispatching loop
 * in WARDuino.cpp.
 *
 * The given module `m` is the currently executing module
 *
 * Returning false breaks this loop and marks the
 * execution as crashed. The exception variable can
 * be filled with an explanation.
 *
 * Returning true continues the loop with the
 * notable exception of end which can terminate
 * the program successfully by setting program_done
 *
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
bool Interpreter::i_instr_block(Module *m, uint8_t *block_ptr) {
    read_LEB_32(&m->pc_ptr);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    auto block_itr = m->block_lookup.find(block_ptr);
    ASSERT(block_itr != m->block_lookup.end(), "could not find block");
    push_block(m, block_itr->second, m->sp);
    return true;
}

/**
 * 0x03
 */
bool Interpreter::i_instr_loop(Module *m, uint8_t *block_ptr) {
    read_LEB_32(&m->pc_ptr);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    push_block(m, m->block_lookup[block_ptr], m->sp);
    return true;
}

/**
 * 0x04 if
 */
bool Interpreter::i_instr_if(Module *m, uint8_t *block_ptr) {
    read_LEB_32(&m->pc_ptr);  // ignore block type
    Block *block = m->block_lookup[block_ptr];

    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    push_block(m, block, m->sp);

    uint32_t cond = m->stack[m->sp--].value.uint32;
    if (cond == 0) {  // if false (I32)
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
    debug("      - cond: 0x%x jump to 0x%x, block: %s\n", cond,
          (uint32_t)(m->pc_ptr - m->bytes), block_repr(block));
#endif
    return true;
}

/**
 * 0x05 else
 */
bool Interpreter::i_instr_else(Module *m) {
    Block *block = m->callstack[m->csp].block;
    m->pc_ptr = block->br_ptr;
#if TRACE
    debug("      - of %s jump to 0x%p\n", block_repr(block), m->pc_ptr);
#endif
    return true;
}

/**
 * 0x0b end
 */
bool Interpreter::i_instr_end(Module *m, bool *prog_done) {
    Block *block = pop_block(m);
    if (block == nullptr) {
        return false;  // an exception (set by pop_block)
    }
#if TRACE
    debug("      - of %s\n", block_repr(block));
#endif
    if (block->block_type == 0x00) {  // Function
#if TRACE
        dbg_warn(
            "  << fn0x%x(%d) %s = %s\n", block->fidx, block->fidx,
            block->export_name ? block->export_name : "",
            block->type->result_count > 0 ? value_repr(&m->stack[m->sp]) : "_");
#endif
        if (m->csp == -1) {
            // Return to top-level
            *prog_done = true;
            return true;  // continue execution but brake dispatch loop
        } else {
            // Keep going at return address
        }
    } else if (block->block_type == 0x01) {  // init_expr
        *prog_done = true;
        return true;  // continue execution but brake dispatch loop
    } else {          // Block
        // End of block/loop/if, keep going
    }
    return true;  // continue execution
}

/**
 * 0x0c br
 */
bool Interpreter::i_instr_br(Module *m) {
    uint32_t depth = read_LEB_32(&m->pc_ptr);
    m->csp -= depth;
    // set to end for pop_block
    m->pc_ptr = m->callstack[m->csp].block->br_ptr;
#if TRACE
    debug("      - to: 0x%p\n", m->pc_ptr);
#endif
    return true;
}

/**
 * 0x0d br_if
 */
bool Interpreter::i_instr_br_if(Module *m) {
    uint32_t depth = read_LEB_32(&m->pc_ptr);

    uint32_t cond = m->stack[m->sp--].value.uint32;
    if (cond) {  // if true
        m->csp -= depth;
        // set to end for pop_block
        m->pc_ptr = m->callstack[m->csp].block->br_ptr;
    }
#if TRACE
    debug("      - depth: 0x%x, cond: 0x%x, to: 0x%p\n", depth, cond,
          m->pc_ptr);
#endif
    return true;
}

/**
 * 0x0e br_table
 */
bool Interpreter::i_instr_br_table(Module *m) {
    uint32_t count = read_LEB_32(&m->pc_ptr);
    if (count > BR_TABLE_SIZE) {
        // TODO: check this prior to runtime
        sprintf(exception, "br_table size %" PRIu32 " exceeds max %d\n", count,
                BR_TABLE_SIZE);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        m->br_table[i] = read_LEB_32(&m->pc_ptr);
    }
    uint32_t depth = read_LEB_32(&m->pc_ptr);

    int32_t didx = m->stack[m->sp--].value.int32;
    if (didx >= 0 && didx < (int32_t)count) {
        depth = m->br_table[didx];
    }

    m->csp -= depth;
    // set to end for pop_block
    m->pc_ptr = m->callstack[m->csp].block->br_ptr;
#if TRACE
    debug("      - count: %d, didx: %d, to: 0x%p\n", count, didx, m->pc_ptr);
#endif
    return true;
}

/**
 * 0x0f return
 */
bool Interpreter::i_instr_return(Module *m) {
    while (m->csp >= 0 && m->callstack[m->csp].block->block_type != 0x00) {
        m->csp--;
    }
    // Set the program count to the end of the function
    // The actual pop_block and return is handled by the end opcode.
    m->pc_ptr = m->callstack[0].block->end_ptr;
#if TRACE
    debug("      - to: 0x%p\n", m->pc_ptr);
#endif
    return true;
}

/**
 * 0x10 call
 */
bool Interpreter::i_instr_call(Module *m) {
    uint32_t fidx = read_LEB_32(&m->pc_ptr);

    if (m->warduino->debugger->isProxied(fidx)) {
        return proxy_call(m, fidx);
    }

    if (fidx < m->import_count) {
        return ((Primitive)m->functions[fidx]->func_ptr)(m);
    } else {
        if (m->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            return false;
        }
        setup_call(m, fidx);  // regular function call
#if TRACE
        debug("      - calling function fidx: %d at: 0x%p\n", fidx, m->pc_ptr);
#endif
    }
    return true;
}

/**
 * 0x11 call_indirect
 */
bool Interpreter::i_instr_call_indirect(Module *m) {
    uint32_t tidx = read_LEB_32(&m->pc_ptr);  // TODO: use tidx?
    (void)tidx;
    read_LEB(&m->pc_ptr, 1);  // reserved immediate
    uint32_t val = m->stack[m->sp--].value.uint32;
    if (m->options.mangle_table_index) {
        // val is the table address + the index (not sized for the
        // pointer size) so get the actual (sized) index
#if TRACE
        debug("      - entries: %p, original val: 0x%x, new val: 0x%x\n",
              m->table.entries, val,
              (uint32_t)((uint64_t)m->table.entries) - val);
#endif
        // val = val - (uint32_t)((uint64_t)m->table.entries & 0xFFFFFFFF);
        val = val - (uint32_t)((uint64_t)m->table.entries);
    }
    if (val >= m->table.maximum) {
        sprintf(exception,
                "undefined element 0x%" PRIx32 " (max: 0x%" PRIx32 ") in table",
                val, m->table.maximum);
        return false;
    }

    uint32_t fidx = m->table.entries[val];
#if TRACE
    debug("       - call_indirect tidx: %d, val: 0x%x, fidx: 0x%x\n", tidx, val,
          fidx);
#endif

    if (fidx < m->import_count) {
        // THUNK thunk_out(m, fidx);    // import/thunk call
    } else {
        Block *func = m->functions[fidx];
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

#if TRACE
        debug(
            "      - tidx: %d, table idx: %d, "
            "calling function fidx: %d at: 0x%p\n",
            tidx, val, fidx, m->pc_ptr);
#endif
    }
    return true;
}

/**
 * 0x1a drop
 * remove a value from the stack
 */
bool Interpreter::i_instr_drop(Module *m) {
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
bool Interpreter::i_instr_select(Module *m) {
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
bool Interpreter::i_instr_get_local(Module *m) {
    int32_t arg = read_LEB_32(&m->pc_ptr);
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg,
          value_repr(&m->stack[m->fp + arg]));
#endif
    m->stack[++m->sp] = m->stack[m->fp + arg];
    return true;
}

/**
 * 0x21 set_local
 */
bool Interpreter::i_instr_set_local(Module *m) {
    int32_t arg = read_LEB_32(&m->pc_ptr);
    m->stack[m->fp + arg] = m->stack[m->sp--];
#if TRACE
    debug("      - arg: 0x%x, to %s (stack loc: %d)\n", arg,
          value_repr(&m->stack[m->sp + 1]), m->fp + arg);
#endif
    return true;
}

/**
 * 0x0d tee_local
 */
bool Interpreter::i_instr_tee_local(Module *m) {
    int32_t arg = read_LEB_32(&m->pc_ptr);
    m->stack[m->fp + arg] = m->stack[m->sp];
#if TRACE
    debug("      - arg: 0x%x, to %s\n", arg, value_repr(&m->stack[m->sp]));
#endif
    return true;
}

/**
 * 0x23 get_global
 */
bool Interpreter::i_instr_get_global(Module *m) {
    int32_t arg = read_LEB_32(&m->pc_ptr);
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg, value_repr(&m->globals[arg]));
#endif
    m->stack[++m->sp] = m->globals[arg];
    return true;
}

/**
 * 0x24 set_global
 */
bool Interpreter::i_instr_set_global(Module *m) {
    uint32_t arg = read_LEB_32(&m->pc_ptr);
    m->globals[arg] = m->stack[m->sp--];
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg, value_repr(&m->stack[m->sp + 1]));
#endif
    return true;
}

/**
 * 0x3f current_memory
 */
bool Interpreter::i_instr_current_memory(Module *m) {
    read_LEB_32(&m->pc_ptr);  // ignore reserved
    m->stack[++m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = m->memory.pages;
    return true;
}

/**
 * 0x40 grow_memory
 */
bool Interpreter::i_instr_grow_memory(Module *m) {
    read_LEB_32(&m->pc_ptr);  // ignore reserved
    uint32_t prev_pages = m->memory.pages;
    uint32_t delta = m->stack[m->sp].value.uint32;
    m->stack[m->sp].value.uint32 = prev_pages;
    if (delta == 0) {
        return true;  // No change
    } else if (delta + prev_pages > m->memory.maximum) {
        m->stack[m->sp].value.uint32 = static_cast<uint32_t>(-1);
        return true;
    }
    m->memory_resize(m->memory.pages + delta);
    return true;
}

void Interpreter::load(Module *m, uint32_t offset, uint32_t addr, int size, uint8_t value_type, bool sign_extend) {
    uint8_t *maddr = m->memory.bytes.data() + offset + addr;
    memcpy(&m->stack[m->sp].value, maddr, size);
}

/**
 * 0x0d XXX
 */
bool Interpreter::i_instr_mem_load(Module *m, uint8_t opcode) {
    bool overflow = false;
    uint8_t *maddr, *mem_end;
    uint32_t flags = read_LEB_32(&m->pc_ptr);
    uint32_t offset = read_LEB_32(&m->pc_ptr);
    uint32_t addr = m->stack[m->sp--].value.uint32;
    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned load - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x\n",
            flags, offset, addr);
    }
    if (offset + addr < addr) {
        overflow = true;
    }
    maddr = m->memory.bytes.data() + offset + addr;
    if (maddr < m->memory.bytes.data()) {
        overflow = true;
    }
    mem_end = m->memory.bytes.data() + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + LOAD_SIZE[opcode - 0x28] > mem_end) {
        overflow = true;
    }
    dbg_info("      - addr: 0x%x, offset: 0x%x, maddr: %p, mem_end: %p\n", addr,
             offset, maddr, mem_end);
    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            dbg_warn("memory start: %p, memory end: %p, maddr: %p\n",
                     m->memory.bytes, mem_end, maddr);
            sprintf(exception, "out of bounds memory access");
            return false;
        }
    }
    m->stack[++m->sp].value.uint64 = 0;  // initialize to 0
    switch (opcode) {
        case 0x28: {
            load(m, offset, addr, 4, I32, false);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load
        }
        case 0x29:
            load(m, offset, addr, 8, I64, false);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load
        case 0x2a:
            load(m, offset, addr, 4, F32, false);
            m->stack[m->sp].value_type = F32;
            break;  // f32.load
        case 0x2b:
            load(m, offset, addr, 8, F64, false);
            m->stack[m->sp].value_type = F64;
            break;  // f64.load
        case 0x2c:
            load(m, offset, addr, 1, I32, true);
            sext_8_32(&m->stack[m->sp].value.uint32);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load8_s
        case 0x2d:
            load(m, offset, addr, 1, I32, false);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load8_u
        case 0x2e:
            load(m, offset, addr, 2, I32, true);
            sext_16_32(&m->stack[m->sp].value.uint32);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load16_s
        case 0x2f:
            load(m, offset, addr, 2, I32, false);
            m->stack[m->sp].value_type = I32;
            break;  // i32.load16_u
        case 0x30:
            load(m, offset, addr, 1, I64, true);
            sext_8_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load8_s
        case 0x31:
            load(m, offset, addr, 1, I64, false);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load8_u
        case 0x32:
            load(m, offset, addr, 2, I64, true);
            sext_16_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load16_s
        case 0x33:
            load(m, offset, addr, 2, I64, false);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load16_u
        case 0x34:
            load(m, offset, addr, 4, I64, true);
            sext_32_64(&m->stack[m->sp].value.uint64);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load32_s
        case 0x35:
            load(m, offset, addr, 4, I64, false);
            m->stack[m->sp].value_type = I64;
            break;  // i64.load32_u
        default:
            return false;
    }
    return true;
}

void Interpreter::store(Module *m, uint32_t offset, uint32_t addr, int value_sp, int size) {
    uint8_t *maddr = m->memory.bytes.data() + offset + addr;
    memcpy(maddr, &m->stack[value_sp].value, size);
}

bool Interpreter::i_instr_mem_store(Module *m, uint8_t opcode) {
    uint8_t *maddr, *mem_end;
    uint32_t flags = read_LEB_32(&m->pc_ptr);
    uint32_t offset = read_LEB_32(&m->pc_ptr);
    int value_sp = m->sp--; // The position of the data we want to store.
    uint32_t addr = m->stack[m->sp--].value.uint32;
    bool overflow = false;

    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned store - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x, val: %s\n",
            flags, offset, addr, value_repr(concrete_stack_value));
    }
    if (offset + addr < addr) {
        overflow = true;
    }
    maddr = m->memory.bytes.data() + offset + addr;
    if (maddr < m->memory.bytes.data()) {
        overflow = true;
    }
    mem_end = m->memory.bytes.data() + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + LOAD_SIZE[opcode - 0x28] > mem_end) {
        overflow = true;
    }
    dbg_info(
        "      - addr: 0x%x, offset: 0x%x, maddr: %p, mem_end: %p, value: "
        "%s\n",
        addr, offset, maddr, mem_end, value_repr(concrete_stack_value));
    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            dbg_warn("memory start: %p, memory end: %p, maddr: %p\n",
                     m->memory.bytes, mem_end, maddr);
            sprintf(exception, "out of bounds memory access");
            return false;
        }
    }
    switch (opcode) {
        case 0x36:
            store(m, offset, addr, value_sp, 4);
            break;  // i32.store
        case 0x37:
            store(m, offset, addr, value_sp, 8);
            break;  // i64.store
        case 0x38:
            store(m, offset, addr, value_sp, 4);
            break;  // f32.store
        case 0x39:
            store(m, offset, addr, value_sp, 8);
            break;  // f64.store
        case 0x3a:
            store(m, offset, addr, value_sp, 1);
            break;  // i32.store8
        case 0x3b:
            store(m, offset, addr, value_sp, 2);
            break;  // i32.store16
        case 0x3c:
            store(m, offset, addr, value_sp, 1);
            break;  // i64.store8
        case 0x3d:
            store(m, offset, addr, value_sp, 2);
            break;  // i64.store16
        case 0x3e:
            store(m, offset, addr, value_sp, 4);
            break;  // i64.store32
        default:
            return false;
    }
    return true;
}

/**
 * 0x41...0x44 const
 */
bool Interpreter::i_instr_const(Module *m, uint8_t opcode) {
    StackValue *target = &m->stack[++m->sp];

    switch (opcode) {
        case 0x41:  // i32.const
            target->value_type = I32;
            target->value.uint32 = read_LEB_signed(&m->pc_ptr, 32);
            break;
        case 0x42:  // i64.const
            target->value_type = I64;
            target->value.int64 = read_LEB_signed(&m->pc_ptr, 64);
            break;
        case 0x43:  // f32.const
            target->value_type = F32;
            memcpy(&target->value.uint32, m->pc_ptr, 4);
            m->pc_ptr += 4;
            break;
        case 0x44:  // f64.const
            target->value_type = F64;
            memcpy(&target->value.uint64, m->pc_ptr, 8);
            m->pc_ptr += 8;
            break;
        default:
            return false;
    }
    return true;
}

/**
 * 0x45 eqz
 */
bool Interpreter::i_instr_unary_u32(Module *m, uint8_t opcode) {
    switch (opcode) {
        case 0x45:  // i32.eqz
            m->stack[m->sp].value.uint32 =
                static_cast<uint32_t>(m->stack[m->sp].value.uint32 == 0);
            break;
        case 0x50:  // i64.eqz
            m->stack[m->sp].value_type = I32;
            m->stack[m->sp].value.uint32 =
                static_cast<uint32_t>(m->stack[m->sp].value.uint64 == 0);
            break;
        default:
            return false;
    }
    return true;
}

/**
 * 0x0d binop32
 */
bool Interpreter::i_instr_math_u32(Module *m, uint8_t opcode) {
    uint32_t a = m->stack[m->sp - 1].value.uint32;
    uint32_t b = m->stack[m->sp].value.uint32;
    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x46:
            c = static_cast<uint32_t>(a == b);
            break;  // i32.eq
        case 0x47:
            c = static_cast<uint32_t>(a != b);
            break;  // i32.ne
        case 0x48:
            c = static_cast<uint32_t>((int32_t)a < (int32_t)b);
            break;  // i32.lt_s
        case 0x49:
            c = static_cast<uint32_t>(a < b);
            break;  // i32.lt_u
        case 0x4a:
            c = static_cast<uint32_t>((int32_t)a > (int32_t)b);
            break;  // i32.gt_s
        case 0x4b:
            c = static_cast<uint32_t>(a > b);
            break;  // i32.gt_u
        case 0x4c:
            c = static_cast<uint32_t>((int32_t)a <= (int32_t)b);
            break;  // i32.le_s
        case 0x4d:
            c = static_cast<uint32_t>(a <= b);
            break;  // i32.le_u
        case 0x4e:
            c = static_cast<uint32_t>((int32_t)a >= (int32_t)b);
            break;  // i32.ge_s
        case 0x4f:
            c = static_cast<uint32_t>(a >= b);
            break;  // i32.ge_u
        default:
            return false;
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

/**
 * 0x0d binop64
 */
bool Interpreter::i_instr_math_u64(Module *m, uint8_t opcode) {
    uint64_t d = m->stack[m->sp - 1].value.uint64;
    uint64_t e = m->stack[m->sp].value.uint64;
    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x51:
            c = static_cast<uint32_t>(d == e);
            break;  // i64.eq
        case 0x52:
            c = static_cast<uint32_t>(d != e);
            break;  // i64.ne
        case 0x53:
            c = static_cast<uint32_t>((int64_t)d < (int64_t)e);
            break;  // i64.lt_s
        case 0x54:
            c = static_cast<uint32_t>(d < e);
            break;  // i64.lt_u
        case 0x55:
            c = static_cast<uint32_t>((int64_t)d > (int64_t)e);
            break;  // i64.gt_s
        case 0x56:
            c = static_cast<uint32_t>(d > e);
            break;  // i64.gt_u
        case 0x57:
            c = static_cast<uint32_t>((int64_t)d <= (int64_t)e);
            break;  // i64.le_s
        case 0x58:
            c = static_cast<uint32_t>(d <= e);
            break;  // i64.le_u
        case 0x59:
            c = static_cast<uint32_t>((int64_t)d >= (int64_t)e);
            break;  // i64.ge_s
        case 0x5a:
            c = static_cast<uint32_t>(d >= e);
            break;  // i64.ge_u
        default:
            return false;
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

/**
 * 0x0d binop64
 */
bool Interpreter::i_instr_math_f32(Module *m, uint8_t opcode) {
    float g = m->stack[m->sp - 1].value.f32;
    float h = m->stack[m->sp].value.f32;
    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x5b:
            c = static_cast<uint32_t>(g == h);
            break;  // f32.eq
        case 0x5c:
            c = static_cast<uint32_t>(g != h);
            break;  // f32.ne
        case 0x5d:
            c = static_cast<uint32_t>(g < h);
            break;  // f32.lt
        case 0x5e:
            c = static_cast<uint32_t>(g > h);
            break;  // f32.gt
        case 0x5f:
            c = static_cast<uint32_t>(g <= h);
            break;  // f32.le
        case 0x60:
            c = static_cast<uint32_t>(g >= h);
            break;  // f32.ge
        default:
            return false;
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

/**
 * 0x0d binopf64
 */
bool Interpreter::i_instr_math_f64(Module *m, uint8_t opcode) {
    double j = m->stack[m->sp - 1].value.f64;
    double k = m->stack[m->sp].value.f64;

    uint32_t c;
    m->sp -= 1;
    switch (opcode) {
        case 0x61:
            c = static_cast<uint32_t>(j == k);
            break;  // f64.eq
        case 0x62:
            c = static_cast<uint32_t>(j != k);
            break;  // f64.ne
        case 0x63:
            c = static_cast<uint32_t>(j < k);
            break;  // f64.lt
        case 0x64:
            c = static_cast<uint32_t>(j > k);
            break;  // f64.gt
        case 0x65:
            c = static_cast<uint32_t>(j <= k);
            break;  // f64.le
        case 0x66:
            c = static_cast<uint32_t>(j >= k);
            break;  // f64.ge
        default:
            return false;
    }
    m->stack[m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = c;
    return true;
}

bool Interpreter::i_instr_unary_i32(Module *m, uint8_t opcode) {
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
        default:
            return false;
    }
    m->stack[m->sp].value.uint32 = c;
    return true;
}

bool Interpreter::i_instr_unary_i64(Module *m, uint8_t opcode) {
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
        default:
            return false;
    }
    m->stack[m->sp].value.uint64 = f;
    return true;
}

/**
 * 0x0d XXX
 */
bool Interpreter::i_instr_unary_floating(Module *m, uint8_t opcode) {
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
        default:
            return false;
    }
    return true;
}

/**
 * 0x0d binary_i32
 */
bool Interpreter::i_instr_binary_i32(Module *m, uint8_t opcode) {
    // TODO: verify if this should not be done with int32_t instead
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
            c = (int32_t)a >> b;  // NOLINT(hicpp-signed-bitwise)
            break;                // i32.shr_s
        case 0x76:
            c = a >> b;
            break;  // i32.shr_u
        case 0x77:
            c = rotl32(a, b);
            break;  // i32.rotl
        case 0x78:
            c = rotr32(a, b);
            break;  // i32.rotr
        default:
            return false;
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
bool Interpreter::i_instr_binary_i64(Module *m, uint8_t opcode) {
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
            f = ((int64_t)d) >> e;  // NOLINT(hicpp-signed-bitwise)
            break;                  // i64.shr_s
        case 0x88:
            f = d >> e;
            break;  // i64.shr_u
        case 0x89:
            f = rotl64(d, e);
            break;  // i64.rotl
        case 0x8a:
            f = rotr64(d, e);
            break;  // i64.rotr
        default:
            return false;
    }
    m->stack[m->sp].value.uint64 = f;

    return true;
}

/**
 * 0x0d XXX
 */
bool Interpreter::i_instr_binary_f32(Module *m, uint8_t opcode) {
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
            i = (float)wa_fmin(g, h);
            break;  // f32.min
        case 0x97:
            i = (float)wa_fmax(g, h);
            break;  // f32.max
        case 0x98:
            i = std::signbit(h) ? -fabs(g) : fabs(g);
            break;  // f32.copysign
        default:
            return false;
    }
    m->stack[m->sp].value.f32 = i;
    return true;
}

/**
 * 0x0d XXX
 */
bool Interpreter::i_instr_binary_f64(Module *m, uint8_t opcode) {
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
            l = std::signbit(k) ? -fabs(j) : fabs(j);
            break;  // f64.copysign
        default:
            return false;
    }
    m->stack[m->sp].value.f64 = l;

    return true;
}

/**
 * 0x0d XXX
 */
bool Interpreter::i_instr_conversion(Module *m, uint8_t opcode) {
    switch (opcode) {
        case 0xa7:
            m->stack[m->sp].value.uint64 &= 0x00000000ffffffff;
            m->stack[m->sp].value_type = I32;
            break;  // i32.wrap/i64
        case 0xa8:
            if (std::isnan(m->stack[m->sp].value.f32)) {
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
            if (std::isnan(m->stack[m->sp].value.f32)) {
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
            if (std::isnan(m->stack[m->sp].value.f64)) {
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
            if (std::isnan(m->stack[m->sp].value.f64)) {
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
            if (std::isnan(m->stack[m->sp].value.f32)) {
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
            if (std::isnan(m->stack[m->sp].value.f32)) {
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
            if (std::isnan(m->stack[m->sp].value.f64)) {
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
            if (std::isnan(m->stack[m->sp].value.f64)) {
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
            m->stack[m->sp].value.f32 = (float)m->stack[m->sp].value.f64;
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
        default:
            return false;
    }

    return true;
}

/**
 * 0xe0 ... 0xe3 callback operations
 */
bool Interpreter::i_instr_callback(Module *m, uint8_t opcode) {
    // TODO
    return true;
}

std::string OPERATOR_INFO[] = {
    // Control flow operators
    "unreachable",           // 0x00
    "nop",                   // 0x01
    "block",                 // 0x02
    "loop",                  // 0x03
    "if",                    // 0x04
    "else",                  // 0x05
    "RESERVED",              // 0x06
    "RESERVED",              // 0x07
    "RESERVED",              // 0x08
    "RESERVED",              // 0x09
    "RESERVED",              // 0x0a
    "end",                   // 0x0b
    "br",                    // 0x0c
    "br_if",                 // 0x0d
    "br_table",              // 0x0e
    "return",                // 0x0f

    // Call operators
    "call",                  // 0x10
    "call_indirect",         // 0x11

    "RESERVED",              // 0x12
    "RESERVED",              // 0x13
    "RESERVED",              // 0x14
    "RESERVED",              // 0x15
    "RESERVED",              // 0x16
    "RESERVED",              // 0x17
    "RESERVED",              // 0x18
    "RESERVED",              // 0x19

    // Parametric operators
    "drop",                  // 0x1a
    "select",                // 0x1b

    "RESERVED",              // 0x1c
    "RESERVED",              // 0x1d
    "RESERVED",              // 0x1e
    "RESERVED",              // 0x1f

    // Variable access
    "local.get",             // 0x20
    "local.set",             // 0x21
    "local.tee",             // 0x22
    "global.get",            // 0x23
    "global.set",            // 0x24

    "RESERVED",              // 0x25
    "RESERVED",              // 0x26
    "RESERVED",              // 0x27

    // Memory-related operator
    "i32.load",              // 0x28
    "i64.load",              // 0x29
    "f32.load",              // 0x2a
    "f64.load",              // 0x2b
    "i32.load8_s",           // 0x2c
    "i32.load8_u",           // 0x2d
    "i32.load16_s",          // 0x2e
    "i32.load16_u",          // 0x2f
    "i64.load8_s",           // 0x30
    "i64.load8_u",           // 0x31
    "i64.load16_s",          // 0x32
    "i64.load16_u",          // 0x33
    "i64.load32_s",          // 0x34
    "i64.load32_u",          // 0x35
    "i32.store",             // 0x36
    "i64.store",             // 0x37
    "f32.store",             // 0x38
    "f64.store",             // 0x39
    "i32.store8",            // 0x3a
    "i32.store16",           // 0x3b
    "i64.store8",            // 0x3c
    "i64.store16",           // 0x3d
    "i64.store32",           // 0x3e
    "current_memory",        // 0x3f
    "grow_memory",           // 0x40

    // Constants
    "i32.const",             // 0x41
    "i64.const",             // 0x42
    "f32.const",             // 0x43
    "f64.const",             // 0x44

    // Comparison operators
    "i32.eqz",               // 0x45
    "i32.eq",                // 0x46
    "i32.ne",                // 0x47
    "i32.lt_s",              // 0x48
    "i32.lt_u",              // 0x49
    "i32.gt_s",              // 0x4a
    "i32.gt_u",              // 0x4b
    "i32.le_s",              // 0x4c
    "i32.le_u",              // 0x4d
    "i32.ge_s",              // 0x4e
    "i32.ge_u",              // 0x4f

    "i64.eqz",               // 0x50
    "i64.eq",                // 0x51
    "i64.ne",                // 0x52
    "i64.lt_s",              // 0x53
    "i64.lt_u",              // 0x54
    "i64.gt_s",              // 0x55
    "i64.gt_u",              // 0x56
    "i64.le_s",              // 0x57
    "i64.le_u",              // 0x58
    "i64.ge_s",              // 0x59
    "i64.ge_u",              // 0x5a

    "f32.eq",                // 0x5b
    "f32.ne",                // 0x5c
    "f32.lt",                // 0x5d
    "f32.gt",                // 0x5e
    "f32.le",                // 0x5f
    "f32.ge",                // 0x60

    "f64.eq",                // 0x61
    "f64.ne",                // 0x62
    "f64.lt",                // 0x63
    "f64.gt",                // 0x64
    "f64.le",                // 0x65
    "f64.ge",                // 0x66

    // Numeric operators
    "i32.clz",               // 0x67
    "i32.ctz",               // 0x68
    "i32.popcnt",            // 0x69
    "i32.add",               // 0x6a
    "i32.sub",               // 0x6b
    "i32.mul",               // 0x6c
    "i32.div_s",             // 0x6d
    "i32.div_u",             // 0x6e
    "i32.rem_s",             // 0x6f
    "i32.rem_u",             // 0x70
    "i32.and",               // 0x71
    "i32.or",                // 0x72
    "i32.xor",               // 0x73
    "i32.shl",               // 0x74
    "i32.shr_s",             // 0x75
    "i32.shr_u",             // 0x76
    "i32.rotl",              // 0x77
    "i32.rotr",              // 0x78

    "i64.clz",               // 0x79
    "i64.ctz",               // 0x7a
    "i64.popcnt",            // 0x7b
    "i64.add",               // 0x7c
    "i64.sub",               // 0x7d
    "i64.mul",               // 0x7e
    "i64.div_s",             // 0x7f
    "i64.div_u",             // 0x80
    "i64.rem_s",             // 0x81
    "i64.rem_u",             // 0x82
    "i64.and",               // 0x83
    "i64.or",                // 0x84
    "i64.xor",               // 0x85
    "i64.shl",               // 0x86
    "i64.shr_s",             // 0x87
    "i64.shr_u",             // 0x88
    "i64.rotl",              // 0x89
    "i64.rotr",              // 0x8a

    "f32.abs",               // 0x8b
    "f32.neg",               // 0x8c
    "f32.ceil",              // 0x8d
    "f32.floor",             // 0x8e
    "f32.trunc",             // 0x8f
    "f32.nearest",           // 0x90
    "f32.sqrt",              // 0x91
    "f32.add",               // 0x92
    "f32.sub",               // 0x93
    "f32.mul",               // 0x94
    "f32.div",               // 0x95
    "f32.min",               // 0x96
    "f32.max",               // 0x97
    "f32.copysign",          // 0x98

    "f64.abs",               // 0x99
    "f64.neg",               // 0x9a
    "f64.ceil",              // 0x9b
    "f64.floor",             // 0x9c
    "f64.trunc",             // 0x9d
    "f64.nearest",           // 0x9e
    "f64.sqrt",              // 0x9f
    "f64.add",               // 0xa0
    "f64.sub",               // 0xa1
    "f64.mul",               // 0xa2
    "f64.div",               // 0xa3
    "f64.min",               // 0xa4
    "f64.max",               // 0xa5
    "f64.copysign",          // 0xa6

    // Conversions
    "i32.wrap_i64",          // 0xa7
    "i32.trunc_f32_s",       // 0xa8
    "i32.trunc_f32_u",       // 0xa9
    "i32.trunc_f64_s",       // 0xaa
    "i32.trunc_f64_u",       // 0xab

    "i64.extend_i32_s",      // 0xac
    "i64.extend_i32_u",      // 0xad
    "i64.trunc_f32_s",       // 0xae
    "i64.trunc_f32_u",       // 0xaf
    "i64.trunc_f64_s",       // 0xb0
    "i64.trunc_f64_u",       // 0xb1

    "f32.convert_i32_s",     // 0xb2
    "f32.convert_i32_u",     // 0xb3
    "f32.convert_i64_s",     // 0xb4
    "f32.convert_i64_u",     // 0xb5
    "f32.demote_f64",        // 0xb6

    "f64.convert_i32_s",     // 0xb7
    "f64.convert_i32_u",     // 0xb8
    "f64.convert_i64_s",     // 0xb9
    "f64.convert_i64_u",     // 0xba
    "f64.promote_f32",       // 0xbb

    // Reinterpretations
    "i32.reinterpret_f32",   // 0xbc
    "i64.reinterpret_f64",   // 0xbd
    "f32.reinterpret_i32",   // 0xbe
    "f64.reinterpret_i64"    // 0xbf
};

/*int INSTR_ARGS[] = {
    // Control flow operators
    0,           // 0x00
    0,                   // 0x01
    0,                 // 0x02
    0,                  // 0x03
    0,                    // 0x04
    0,                  // 0x05
    -1,              // 0x06
    -1,              // 0x07
    -1,              // 0x08
    -1,              // 0x09
    -1,              // 0x0a
    0,                   // 0x0b
    1,                    // 0x0c
    1,                 // 0x0d
    -2,              // 0x0e
    0,                // 0x0f

    // Call operators
    1,                  // 0x10
    0,         // 0x11

    -1,              // 0x12
    -1,              // 0x13
    -1,              // 0x14
    -1,              // 0x15
    -1,              // 0x16
    -1,              // 0x17
    -1,              // 0x18
    -1,              // 0x19

    // Parametric operators
    0,                  // 0x1a
    0,                // 0x1b

    -1,              // 0x1c
    -1,              // 0x1d
    -1,              // 0x1e
    -1,              // 0x1f

    // Variable access
    1,             // 0x20
    1,             // 0x21
    1,             // 0x22
    1,            // 0x23
    1,            // 0x24

    -1,              // 0x25
    -1,              // 0x26
    -1,              // 0x27

    // Memory-related operator
    "i32.load",              // 0x28
    "i64.load",              // 0x29
    "f32.load",              // 0x2a
    "f64.load",              // 0x2b
    "i32.load8_s",           // 0x2c
    "i32.load8_u",           // 0x2d
    "i32.load16_s",          // 0x2e
    "i32.load16_u",          // 0x2f
    "i64.load8_s",           // 0x30
    "i64.load8_u",           // 0x31
    "i64.load16_s",          // 0x32
    "i64.load16_u",          // 0x33
    "i64.load32_s",          // 0x34
    "i64.load32_u",          // 0x35
    "i32.store",             // 0x36
    "i64.store",             // 0x37
    "f32.store",             // 0x38
    "f64.store",             // 0x39
    "i32.store8",            // 0x3a
    "i32.store16",           // 0x3b
    "i64.store8",            // 0x3c
    "i64.store16",           // 0x3d
    "i64.store32",           // 0x3e
    "current_memory",        // 0x3f
    "grow_memory",           // 0x40

    // Constants
    "i32.const",             // 0x41
    "i64.const",             // 0x42
    "f32.const",             // 0x43
    "f64.const",             // 0x44

    // Comparison operators
    "i32.eqz",               // 0x45
    "i32.eq",                // 0x46
    "i32.ne",                // 0x47
    "i32.lt_s",              // 0x48
    "i32.lt_u",              // 0x49
    "i32.gt_s",              // 0x4a
    "i32.gt_u",              // 0x4b
    "i32.le_s",              // 0x4c
    "i32.le_u",              // 0x4d
    "i32.ge_s",              // 0x4e
    "i32.ge_u",              // 0x4f

    "i64.eqz",               // 0x50
    "i64.eq",                // 0x51
    "i64.ne",                // 0x52
    "i64.lt_s",              // 0x53
    "i64.lt_u",              // 0x54
    "i64.gt_s",              // 0x55
    "i64.gt_u",              // 0x56
    "i64.le_s",              // 0x57
    "i64.le_u",              // 0x58
    "i64.ge_s",              // 0x59
    "i64.ge_u",              // 0x5a

    "f32.eq",                // 0x5b
    "f32.ne",                // 0x5c
    "f32.lt",                // 0x5d
    "f32.gt",                // 0x5e
    "f32.le",                // 0x5f
    "f32.ge",                // 0x60

    "f64.eq",                // 0x61
    "f64.ne",                // 0x62
    "f64.lt",                // 0x63
    "f64.gt",                // 0x64
    "f64.le",                // 0x65
    "f64.ge",                // 0x66

    // Numeric operators
    "i32.clz",               // 0x67
    "i32.ctz",               // 0x68
    "i32.popcnt",            // 0x69
    "i32.add",               // 0x6a
    "i32.sub",               // 0x6b
    "i32.mul",               // 0x6c
    "i32.div_s",             // 0x6d
    "i32.div_u",             // 0x6e
    "i32.rem_s",             // 0x6f
    "i32.rem_u",             // 0x70
    "i32.and",               // 0x71
    "i32.or",                // 0x72
    "i32.xor",               // 0x73
    "i32.shl",               // 0x74
    "i32.shr_s",             // 0x75
    "i32.shr_u",             // 0x76
    "i32.rotl",              // 0x77
    "i32.rotr",              // 0x78

    "i64.clz",               // 0x79
    "i64.ctz",               // 0x7a
    "i64.popcnt",            // 0x7b
    "i64.add",               // 0x7c
    "i64.sub",               // 0x7d
    "i64.mul",               // 0x7e
    "i64.div_s",             // 0x7f
    "i64.div_u",             // 0x80
    "i64.rem_s",             // 0x81
    "i64.rem_u",             // 0x82
    "i64.and",               // 0x83
    "i64.or",                // 0x84
    "i64.xor",               // 0x85
    "i64.shl",               // 0x86
    "i64.shr_s",             // 0x87
    "i64.shr_u",             // 0x88
    "i64.rotl",              // 0x89
    "i64.rotr",              // 0x8a

    "f32.abs",               // 0x8b
    "f32.neg",               // 0x8c
    "f32.ceil",              // 0x8d
    "f32.floor",             // 0x8e
    "f32.trunc",             // 0x8f
    "f32.nearest",           // 0x90
    "f32.sqrt",              // 0x91
    "f32.add",               // 0x92
    "f32.sub",               // 0x93
    "f32.mul",               // 0x94
    "f32.div",               // 0x95
    "f32.min",               // 0x96
    "f32.max",               // 0x97
    "f32.copysign",          // 0x98

    "f64.abs",               // 0x99
    "f64.neg",               // 0x9a
    "f64.ceil",              // 0x9b
    "f64.floor",             // 0x9c
    "f64.trunc",             // 0x9d
    "f64.nearest",           // 0x9e
    "f64.sqrt",              // 0x9f
    "f64.add",               // 0xa0
    "f64.sub",               // 0xa1
    "f64.mul",               // 0xa2
    "f64.div",               // 0xa3
    "f64.min",               // 0xa4
    "f64.max",               // 0xa5
    "f64.copysign",          // 0xa6

    // Conversions
    "i32.wrap_i64",          // 0xa7
    "i32.trunc_f32_s",       // 0xa8
    "i32.trunc_f32_u",       // 0xa9
    "i32.trunc_f64_s",       // 0xaa
    "i32.trunc_f64_u",       // 0xab

    "i64.extend_i32_s",      // 0xac
    "i64.extend_i32_u",      // 0xad
    "i64.trunc_f32_s",       // 0xae
    "i64.trunc_f32_u",       // 0xaf
    "i64.trunc_f64_s",       // 0xb0
    "i64.trunc_f64_u",       // 0xb1

    "f32.convert_i32_s",     // 0xb2
    "f32.convert_i32_u",     // 0xb3
    "f32.convert_i64_s",     // 0xb4
    "f32.convert_i64_u",     // 0xb5
    "f32.demote_f64",        // 0xb6

    "f64.convert_i32_s",     // 0xb7
    "f64.convert_i32_u",     // 0xb8
    "f64.convert_i64_s",     // 0xb9
    "f64.convert_i64_u",     // 0xba
    "f64.promote_f32",       // 0xbb

    // Reinterpretations
    "i32.reinterpret_f32",   // 0xbc
    "i64.reinterpret_f64",   // 0xbd
    "f32.reinterpret_i32",   // 0xbe
    "f64.reinterpret_i64"    // 0xbf
};*/

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

        // Resolve 1 callback event if queue is not empty and VM not paused, and
        // no event currently resolving
        CallbackHandler::resolve_event();

        // Sleep interpret loop if 'paused' or 'waiting drone'
        if (m->warduino->program_state == WARDUINOpause ||
            m->warduino->program_state == PROXYhalt) {
            // wait until new debug messages arrive
            if (m->warduino->program_state == WARDUINOpause) {
                std::unique_lock<std::mutex> lock(
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

        opcode = *m->pc_ptr;
        block_ptr = m->pc_ptr;
        m->pc_ptr += 1;

        dbg_dump_stack(m);
        dbg_trace(" PC: %p OPCODE: <%s> in %s\n", block_ptr,
                  opcode_repr(opcode),
                  m->pc_ptr > m->bytes && m->pc_ptr < m->bytes + m->byte_count
                      ? "module"
                      : "patch");

        /*std::string symbolic_stack_str = "sym stack    : [ ";
        std::string stack_str = "stack        : [ ";
        //for (int i = std::max(0, m->sp - 5); i <= m->sp; i++) {
        for (int i = 0; i <= m->sp; i++) {
            stack_str += std::to_string(m->stack[i].value.int32);
            if (m->symbolic_stack[i].has_value()) {
                auto v = m->symbolic_stack[i].value().simplify();
                assert(v.to_string() != "null");
                if (v.is_bv() && v.is_numeral()) {
                    symbolic_stack_str += std::to_string(v.get_numeral_uint64());
                } else {
                    symbolic_stack_str += v.to_string();
                }
            }
            else {
                // None means that there is no symbolic value for the current thing on the stack, we didn't implement something yet in that case.
                //assert(false);
                symbolic_stack_str += "none";
            }
            if (i != m->sp) {
                symbolic_stack_str += ", ";
                stack_str += ", ";
            }
        }
        symbolic_stack_str += " ]";
        stack_str += " ]";
        std::cout << symbolic_stack_str << std::endl;
        std::cout << stack_str << std::endl;*/
        /*std::cout << "running instr: " << OPERATOR_INFO[opcode]; // << std::endl;
        uint8_t *pc_ptr_tmp = m->pc_ptr;
        std::cout << " " << read_LEB_32(&pc_ptr_tmp) << std::endl;*/
        m->instructions_executed++;
        if (m->max_instructions > 0 && m->instructions_executed > m->max_instructions) {
            std::cout << "Max instructions executed!" << std::endl;
            return true;
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
                success &= i_instr_block(m, block_ptr);
                continue;
            case 0x03:  // loop
                success &= i_instr_loop(m, block_ptr);
                continue;
            case 0x04:  // if
                success &= i_instr_if(m, block_ptr);
                continue;
            case 0x05:  // else
                success &= i_instr_else(m);
                continue;
            case 0x0b:  // end
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
                sprintf(exception, "unrecognized opcode 0x%x", opcode);
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
    while (CallbackHandler::resolving_event && CallbackHandler::resolve_event())
        ;

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