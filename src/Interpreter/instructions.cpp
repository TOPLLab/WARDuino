#include "instructions.h"

#include <cmath>
#include <cstring>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/CallbackHandler.h"

// performs proxy calls to an MCU
bool proxy_call(Module *m, uint32_t fidx) {
    dbg_info("Remote Function Call %d\n", fidx);
    ProxySupervisor *supervisor = m->warduino->debugger->supervisor;
    RFC *rfc;
    Type *type = m->functions[fidx].type;
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
bool i_instr_block(Module *m, uint8_t *block_ptr) {
    read_LEB_32(&m->pc_ptr);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    auto block_itr = m->block_lookup.find(block_ptr);
    ASSERT(block_itr != m->block_lookup.end(), "could not find block");
    m->warduino->interpreter->push_block(m, block_itr->second, m->sp);
    return true;
}

/**
 * 0x03
 */
bool i_instr_loop(Module *m, uint8_t *block_ptr) {
    read_LEB_32(&m->pc_ptr);  // ignore block type
    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    m->warduino->interpreter->push_block(m, m->block_lookup[block_ptr], m->sp);
    return true;
}

/**
 * 0x04 if
 */
bool i_instr_if(Module *m, uint8_t *block_ptr) {
    read_LEB_32(&m->pc_ptr);  // ignore block type
    Block *block = m->block_lookup[block_ptr];

    if (m->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    m->warduino->interpreter->push_block(m, block, m->sp);

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
bool i_instr_else(Module *m) {
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
bool i_instr_end(Module *m, bool *prog_done) {
    Block *block = m->warduino->interpreter->pop_block(m);
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
bool i_instr_br(Module *m) {
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
bool i_instr_br_if(Module *m) {
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
bool i_instr_br_table(Module *m) {
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
bool i_instr_return(Module *m) {
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
bool i_instr_call(Module *m) {
    uint32_t fidx = read_LEB_32(&m->pc_ptr);

    if (m->warduino->debugger->isProxied(fidx)) {
        return proxy_call(m, fidx);
    }

    if (fidx < m->import_count) {
        return ((Primitive)m->functions[fidx].func_ptr)(m);
    } else {
        if (m->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            return false;
        }
        m->warduino->interpreter->setup_call(m, fidx);  // regular function call
#if TRACE
        debug("      - calling function fidx: %d at: 0x%p\n", fidx, m->pc_ptr);
#endif
    }
    return true;
}

/**
 * 0x11 call_indirect
 */
bool i_instr_call_indirect(Module *m) {
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

        m->warduino->interpreter->setup_call(m, fidx);  // regular function call

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
bool i_instr_drop(Module *m) {
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
bool i_instr_select(Module *m) {
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
bool i_instr_get_local(Module *m) {
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
bool i_instr_set_local(Module *m) {
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
bool i_instr_tee_local(Module *m) {
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
bool i_instr_get_global(Module *m) {
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
bool i_instr_set_global(Module *m) {
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
bool i_instr_current_memory(Module *m) {
    read_LEB_32(&m->pc_ptr);  // ignore reserved
    m->stack[++m->sp].value_type = I32;
    m->stack[m->sp].value.uint32 = m->memory.pages;
    return true;
}

/**
 * 0x40 grow_memory
 */
bool i_instr_grow_memory(Module *m) {
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
    m->memory.pages += delta;
    m->memory.bytes = (uint8_t *)arecalloc(
        m->memory.bytes, prev_pages * PAGE_SIZE, m->memory.pages * PAGE_SIZE,
        1 /*sizeof(uint32_t)*/, "Module->memory.bytes", true);
    return true;
}

/**
 * 0x0d XXX
 */
bool i_instr_mem_load(Module *m, uint8_t opcode) {
    uint32_t flags = read_LEB_32(&m->pc_ptr);
    uint32_t offset = read_LEB_32(&m->pc_ptr);
    uint32_t addr = m->stack[m->sp--].value.uint32;
    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned load - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x\n",
            flags, offset, addr);
    }

    return m->warduino->interpreter->load(m, I32 + (0x28 - opcode), addr,
                                          offset);
}

bool i_instr_mem_store(Module *m, uint8_t opcode) {
    StackValue *sval = &m->stack[m->sp--];
    uint32_t flags = read_LEB_32(&m->pc_ptr);
    uint32_t offset = read_LEB_32(&m->pc_ptr);

    uint32_t addr = m->stack[m->sp--].value.uint32;

    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned store - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x, val: %s\n",
            flags, offset, addr, value_repr(sval));
    }

    if (offset + addr < addr && !m->options.disable_memory_bounds) {
        m->warduino->interpreter->report_overflow(
            m, m->memory.bytes + offset + addr);
    }

    addr += offset;
    return m->warduino->interpreter->store(m, I32 + (0x36 - opcode), addr,
                                           *sval);
}

/**
 * 0x41...0x44 const
 */
bool i_instr_const(Module *m, uint8_t opcode) {
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
bool i_instr_unary_u32(Module *m, uint8_t opcode) {
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
bool i_instr_math_u32(Module *m, uint8_t opcode) {
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
bool i_instr_math_u64(Module *m, uint8_t opcode) {
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
bool i_instr_math_f32(Module *m, uint8_t opcode) {
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
bool i_instr_math_f64(Module *m, uint8_t opcode) {
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

bool i_instr_unary_i32(Module *m, uint8_t opcode) {
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

bool i_instr_unary_i64(Module *m, uint8_t opcode) {
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
bool i_instr_unary_floating(Module *m, uint8_t opcode) {
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
bool i_instr_binary_i32(Module *m, uint8_t opcode) {
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
bool i_instr_binary_i64(Module *m, uint8_t opcode) {
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
bool i_instr_binary_f32(Module *m, uint8_t opcode) {
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
bool i_instr_binary_f64(Module *m, uint8_t opcode) {
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
bool i_instr_conversion(Module *m, uint8_t opcode) {
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
bool i_instr_callback(Module *m, uint8_t opcode) {
    // TODO
    return true;
}

/**
 * [0xfd] 0x0f...0x14 SIMD splat operations
 */
bool i_instr_simd_splat(Module* m, uint8_t opcode) {
    auto &raw_top = m->stack[m->sp];
    auto &top = raw_top.value;

    switch(opcode) {
        case 0x0f: { // i8x16.splat
            const int8_t val = top.int32;
            for(auto &i : top.simd.i8x16) i = val;
            break;
        }

        case 0x10: { // i16x8.splat
            const int16_t val = top.int32;
            for(auto &i : top.simd.i16x8) i = val;
            break;
        }

        case 0x11: { // i32x4.splat
            const int32_t val = top.int32;
            for(auto &i : top.simd.i32x4) i = val;
            break;
        }

        case 0x12: { // i64x2.splat
            const int64_t val = top.int64;
            for(auto &i : top.simd.i64x2) i = val;
            break;
        }

        case 0x13: { // f32x4.splat
            const float val = top.f32;
            for(auto &i : top.simd.f32x4) i = val;
            break;
        }

        case 0x14: { // f64x2.splat
            const double val = top.f64;
            for(auto &i : top.simd.f64x2) i = val;
            break;
        }
    }

    raw_top.value_type = V128;
    return true;
}

bool i_instr_simd_extract(Module* m, uint8_t opcode){
    // inner helper to check if lane is within bounds, then execute the operation
    auto &raw_top = m->stack[m->sp];
    const uint8_t lane = *m->pc_ptr;
    m->pc_ptr++;

    // expect the compiler to inline the lambda - see
    // https://stackoverflow.com/questions/13722426/why-can-lambdas-be-better-optimized-by-the-compiler-than-plain-functions/13722515#13722515
    auto lane_handler = [&raw_top, lane]<typename I, typename O>(const int max, I *in, O &out, const uint8_t type) {
        if(lane > max) {
            sprintf(exception, "lane index out of bounds (%d > %d)", lane, max);
            return false;
        }

        out = in[lane];
        raw_top.value_type = type;

        return true;
    };

    auto lane_handler_unsigned = [&raw_top, lane]<typename I, typename O>(const int max, I *in, O &out, const uint8_t type) {
        if(lane > max) {
            sprintf(exception, "lane index out of bounds (%d > %d)", lane, max);
            return false;
        }

        // type switcher between uint8 and uint16 based on input type
        using temp_t = std::conditional_t<std::is_same_v<I, int8_t>, uint8_t, uint16_t>;
        temp_t x;
        memcpy(&x, &in[lane], sizeof(temp_t));
        out = x;
        raw_top.value_type = type;

        return true;
    };

    switch(opcode) {
        case 0x15:  // i8x16.extract_lane_s
            return lane_handler(15, raw_top.value.simd.i8x16, raw_top.value.int32, I32);
        case 0x16: // i8x16.extract_lane_u
            return lane_handler_unsigned(15, raw_top.value.simd.i8x16, raw_top.value.uint32, I32);

        case 0x18:  // i16x8.extract_lane_s
            return lane_handler(7, raw_top.value.simd.i16x8, raw_top.value.int32, I32);
        case 0x19: // i16x8.extract_lane_u
            return lane_handler_unsigned(7, raw_top.value.simd.i16x8, raw_top.value.uint32, I32);

        case 0x1b: // i32x4.extract_lane
            return lane_handler(3, raw_top.value.simd.i32x4, raw_top.value.uint32, I32);

        case 0x1d: // i64x2.extract_lane
            return lane_handler(1, raw_top.value.simd.i64x2, raw_top.value.uint64, I64);

        case 0x1f: // f32x4.extract_lane
            return lane_handler(3, raw_top.value.simd.f32x4, raw_top.value.f32, F32);

        case 0x21: // f64x2.extract_lane
            return lane_handler(1, raw_top.value.simd.f64x2, raw_top.value.f64, F64);
    }

    return false;
}

bool i_instr_simd_replace(Module *m, uint8_t opcode) {
    auto &v128 = m->stack[m->sp - 1];
    auto &update = m->stack[m->sp].value;
    const uint8_t lane = *m->pc_ptr;
    m->pc_ptr++;
    m->sp -= 1;

    auto lane_handler = [&v128, lane]<typename O>(const int max, O *out, O replace) {
        if(lane > max) {
            sprintf(exception, "lane index out of bounds (%d > %d)", lane, max);
            return false;
        }

        out[lane] = replace;
        return true;
    };

    switch(opcode) {
        case 0x17: // i8x16.replace_lane
            return lane_handler(15, v128.value.simd.i8x16, static_cast<int8_t>(update.uint32));
        case 0x1a: // i16x8.replace_lane
            return lane_handler(8, v128.value.simd.i16x8, static_cast<int16_t>(update.uint32));
        case 0x1c: // i32x4.replace_lane
            return lane_handler(4, v128.value.simd.i32x4, static_cast<int32_t>(update.uint32));
        case 0x1e: // i64x2.replace_lane
            return lane_handler(2, v128.value.simd.i64x2, static_cast<int64_t>(update.uint64));
        case 0x20: // f32x4.replace_lane
            return lane_handler(4, v128.value.simd.f32x4, update.f32);
        case 0x22: // f64x2.replace_lane
            return lane_handler(2, v128.value.simd.f64x2, update.f64);
    }

    return false;
}

inline bool verify_endian() {
    static bool _ = [] {
        int n = 1;
        if(*reinterpret_cast<char *>(&n) == 1) {
            // little endian
            return true;
        }

        FATAL("v128.const only supported on little-endian systems");
        return false;
    }();
    return _;
}

bool i_instr_simd_const(Module* m){
    verify_endian();

    const uint8_t *data = m->pc_ptr;
    m->pc_ptr += 16; // skip immediate 16-byte data

    m->sp++;
    auto &v = m->stack[m->sp].value.simd;
    std::memcpy(&v, data, 16);
    m->stack[m->sp].value_type = V128;
    return true;
}

bool i_instr_simd_store(Module* m){
    auto &sv = m->stack[m->sp--];
    StackValue sv2;
    sv2.value_type = I64;
    sv2.value.uint64 = sv.value.simd.i64x2[1];

    const uint32_t flags = read_LEB_32(&m->pc_ptr);
    const uint32_t offset = read_LEB_32(&m->pc_ptr);

    uint32_t ptr = m->stack[m->sp--].value.uint32;

    if(flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned store - flags: 0x%x, offset: 0x%x, addr: 0x%x, val: %s\n",
            flags, offset, ptr, value_repr(&sv)
        );
    }

    if(offset + ptr < ptr && !m->options.disable_memory_bounds) {
        Interpreter::report_overflow(m, m->memory.bytes + offset + ptr);
    }

    ptr += offset;

    // store in 2 consecutive locations
    return m->warduino->interpreter->store(m, I64, ptr, sv) &&
        m->warduino->interpreter->store(m, I64, ptr + 8, sv2);
}

bool i_instr_simd_load(Module* m) {
    const uint32_t flags = read_LEB_32(&m->pc_ptr);
    const uint32_t offset = read_LEB_32(&m->pc_ptr);
    uint32_t ptr = m->stack[m->sp--].value.uint32;
    if(flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned load - flags: 0x%x, offset: 0x%x, addr: 0x%x, val: %s\n",
            flags, offset, ptr
        );
    }

    if(offset + ptr < ptr && !m->options.disable_memory_bounds) {
        Interpreter::report_overflow(m, m->memory.bytes + offset + ptr);
    }

    ptr += offset;

    // load from 2 consecutive locations
    bool success = m->warduino->interpreter->load(m, I64, ptr, offset);
    const auto i64_0 = m->stack[m->sp].value.int64;
    m->sp--; // make sure we overwrite the previous load
    success &= m->warduino->interpreter->load(m, I64, ptr + 8, offset);
    const auto i64_1 = m->stack[m->sp].value.int64;

    // reconstruct v128
    auto & [value_type, value] = m->stack[m->sp];
    value_type = V128;
    value.simd.i64x2[0] = i64_0;
    value.simd.i64x2[1] = i64_1;

    return success;
}

bool i_instr_simd_bin_bit_op(Module* m, uint8_t opcode){
    auto v1 = m->stack[m->sp - 1].value.simd;
    auto v2 = m->stack[m->sp].value.simd;
    m->sp--;

    const auto apply = [&v1, &v2, m]<typename F>(F &&op) {
        m->stack[m->sp].value.simd.i64x2[0] = op(v1.i64x2[0], v2.i64x2[0]);
        m->stack[m->sp].value.simd.i64x2[1] = op(v1.i64x2[1], v2.i64x2[1]);
        return true;
    };

    switch(opcode) {
        case 0x4e: // bit-and
            return apply([](const uint64_t &a, const uint64_t &b) { return a & b; });
        case 0x4f: // bit-and-not
            return apply([](const uint64_t &a, const uint64_t &b) { return a & ~b; });
        case 0x50: // bit-or
            return apply([](const uint64_t &a, const uint64_t &b) { return a | b; });
        case 0x51: // xor
            return apply([](const uint64_t &a, const uint64_t &b) { return a ^ b; });
    }

    return false;
}

bool i_instr_simd_v128_not(Module* m){
    m->stack[m->sp].value.simd.i64x2[0] = ~m->stack[m->sp].value.simd.i64x2[0];
    m->stack[m->sp].value.simd.i64x2[1] = ~m->stack[m->sp].value.simd.i64x2[1];
    return true;
}


// to implement the whole swathe of SIMD instructions, let's use some templates
// so the compiler can generate the code...
namespace {
// anonymous namespace to hide implementation details/allow code folding
// the SIMD vector type (for ease of coding)
using simd_t = decltype(StackValue::value.simd);

// use type-traits for all compile-time meta-data
template <typename T> struct v128_traits;
template <> struct v128_traits<int8_t> {
    // we need to know the number of elements
    constexpr static uint N = 16;
    // we need to know the amount of bits
    constexpr static uint bits = 8;
    // the type of elements in the SIMD vector
    using type = int8_t;
    // the type to use when an operation returns a boolean
    using bool_alt = int8_t;
    // we need to know the signed version of this type
    using signed_t = int8_t;
    // we also need to know the unsigned version of this type
    using unsigned_t = uint8_t;
    // for the ext_add_pairwise, we need the extended type
    using extended_t = int16_t;
    // pointer-to-member to access the SIMD vector
    constexpr static auto member = &simd_t::i8x16;
};
template <> struct v128_traits<uint8_t> {
    constexpr static uint N = 16;
    constexpr static uint bits = 8;
    using type = uint8_t;
    using bool_alt = int8_t;
    using signed_t = int8_t;
    using unsigned_t = uint8_t;
    using extended_t = uint16_t;
    constexpr static auto member = &simd_t::i8x16;
};
template <> struct v128_traits<int16_t> {
    constexpr static uint N = 8;
    constexpr static uint bits = 16;
    using type = int16_t;
    using bool_alt = int16_t;
    using signed_t = int16_t;
    using unsigned_t = uint16_t;
    using extended_t = int32_t;
    constexpr static auto member = &simd_t::i16x8;
};
template <> struct v128_traits<uint16_t> {
    constexpr static uint N = 8;
    constexpr static uint bits = 16;
    using type = uint16_t;
    using bool_alt = int16_t;
    using signed_t = int16_t;
    using unsigned_t = uint16_t;
    using extended_t = uint32_t;
    constexpr static auto member = &simd_t::i16x8;
};
template <> struct v128_traits<int32_t> {
    constexpr static uint N = 4;
    constexpr static uint bits = 32;
    using type = int32_t;
    using bool_alt = int32_t;
    using signed_t = int32_t;
    using unsigned_t = uint32_t;
    constexpr static auto member = &simd_t::i32x4;
};
template <> struct v128_traits<uint32_t> {
    constexpr static uint N = 4;
    constexpr static uint bits = 32;
    using type = uint32_t;
    using bool_alt = int32_t;
    using signed_t = int32_t;
    using unsigned_t = uint32_t;
    constexpr static auto member = &simd_t::i32x4;
};
template <> struct v128_traits<int64_t> {
    constexpr static uint N = 2;
    constexpr static uint bits = 64;
    using type = int64_t;
    using bool_alt = int64_t;
    using signed_t = int64_t;
    using unsigned_t = uint64_t;
    constexpr static auto member = &simd_t::i64x2;
};
template <> struct v128_traits<uint64_t> {
    constexpr static uint N = 2;
    constexpr static uint bits = 64;
    using type = uint64_t;
    using bool_alt = int64_t;
    using signed_t = int64_t;
    using unsigned_t = uint64_t;
    constexpr static auto member = &simd_t::i64x2;
};
template <> struct v128_traits<float> {
    constexpr static uint N = 4;
    using type = float;
    using bool_alt = int32_t;
    using signed_t = float;
    constexpr static auto member = &simd_t::f32x4;
};
template <> struct v128_traits<double> {
    constexpr static uint N = 2;
    using type = double;
    using bool_alt = int64_t;
    using signed_t = double;
    constexpr static auto member = &simd_t::f64x2;
};

// get a reference to a lane in a SIMD vector
template <typename T>
constexpr typename v128_traits<T>::signed_t &get_lane_ref(StackValue &sv, uint8_t lane) {
    // v128_traits<T>::member is pointer-to-member
    // (struct).*(ptr-to-member) gets struct.member
    // so we get (simd-union).(correct type field)[lane]
    return (sv.value.simd.*v128_traits<T>::member)[lane];
}
// get a const reference to a lane in a SIMD vector
template <typename T>
constexpr const typename v128_traits<T>::signed_t &get_lane_cref(const StackValue &sv, uint8_t lane) {
    return (sv.value.simd.*v128_traits<T>::member)[lane];
}
// get a copy of a lane in a SIMD vector
template <typename T>
constexpr typename v128_traits<T>::signed_t get_lane_copy(StackValue &sv, uint8_t lane) {
    return (sv.value.simd.*v128_traits<T>::member)[lane];
}

// apply binary operator lane-wise
template <typename T, typename ExplicitCast = T, typename F>
constexpr bool simd_bin_op(Module *m, const StackValue &sv1, const StackValue &sv2, F &&f) {
    static_assert(std::is_invocable_v<F, ExplicitCast, ExplicitCast>, "Operation is not invocable on type T");
    static_assert(sizeof(T) == sizeof(ExplicitCast), "ExplicitCast must have same size as T");

    using result_t = std::conditional_t<
        std::is_same_v<std::invoke_result_t<F, ExplicitCast, ExplicitCast>, bool>, // if the result is a boolean
        typename v128_traits<T>::bool_alt, // then use the bool_alt type (to ensure lane count for floats)
        typename v128_traits<T>::type // otherwise, use the normal type
    >;
    using result_traits = v128_traits<result_t>; // v128 traits for the result type

    static_assert(v128_traits<T>::N == result_traits::N, "Result type must have same lane count as input type");

    m->stack[m->sp].value_type = V128;
    for(uint i = 0; i < v128_traits<T>::N; i++) {
        ExplicitCast lane1, lane2;
        if constexpr(std::is_same_v<T, ExplicitCast>) {
            lane1 = get_lane_cref<T>(sv1, i);
            lane2 = get_lane_cref<T>(sv2, i);
        }
        else {
            memcpy(&lane1, &get_lane_cref<T>(sv1, i), sizeof(T));
            memcpy(&lane2, &get_lane_cref<T>(sv2, i), sizeof(T));
        }
        get_lane_ref<result_t>(m->stack[m->sp], i) = f(lane1, lane2);
    }
    return true;
}

template <typename T, template <typename> typename FPre, typename ExplicitCast = T, typename F = FPre<ExplicitCast>>
constexpr bool simd_bin_op(Module *m, const StackValue &sv1, const StackValue &sv2) {
    return simd_bin_op<T, ExplicitCast, F>(m, sv1, sv2, F{});
}

template <typename T, template <typename> typename FPre, typename ExplicitCast = T, typename F = FPre<ExplicitCast>>
constexpr bool simd_shift(Module *m, const StackValue &sv1, const StackValue &sv2) {
    static_assert(std::is_invocable_v<F, ExplicitCast, int32_t>, "Operation is not invocable on type T");
    static_assert(sizeof(T) == sizeof(ExplicitCast), "ExplicitCast must have same size as T");

    using result_t = std::conditional_t<
        std::is_same_v<std::invoke_result_t<F, ExplicitCast, int32_t>, bool>, // if the result is a boolean
        typename v128_traits<T>::bool_alt, // then use the bool_alt type (to ensure lane count for floats)
        typename v128_traits<T>::type // otherwise, use the normal type
    >;
    using result_traits = v128_traits<result_t>; // v128 traits for the result type

    static_assert(v128_traits<T>::N == result_traits::N, "Result type must have same lane count as input type");

    F f{};

    m->stack[m->sp].value_type = V128;
    for(uint i = 0; i < v128_traits<T>::N; i++) {
        ExplicitCast lane1;
        int32_t shift = sv2.value.int32;
        if constexpr(std::is_same_v<T, ExplicitCast>) {
            lane1 = get_lane_cref<T>(sv1, i);
        }
        else {
            memcpy(&lane1, &get_lane_cref<T>(sv1, i), sizeof(T));
        }
        get_lane_ref<result_t>(m->stack[m->sp], i) = f(lane1, shift);
    }
    return true;
}

template <typename T>
constexpr bool simd_ext_add(Module *m) {
    using T_traits = v128_traits<T>;
    using extend_t = typename T_traits::extended_t;
    using extend_traits = v128_traits<extend_t>;

    StackValue copy = m->stack[m->sp];
    m->stack[m->sp].value_type = V128;
    StackValue &res = m->stack[m->sp];

    for(uint i = 0; i < extend_traits::N; i++) {
        auto x1 = static_cast<extend_t>(get_lane_cref<T>(copy, 2 * i));
        auto x2 = static_cast<extend_t>(get_lane_cref<T>(copy, 2 * i + 1));
        get_lane_ref<extend_t>(res, i) = x1 + x2;
    }

    return true;
}
}

// helper code for saturating arithmetic
namespace {
template <typename T>
struct sat_add {
    constexpr T operator()(const T t1, const T t2) const {
        // from https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/sat_arith.h#L49
        T t3{};
        if(!__builtin_add_overflow(t1, t2, &t3)) return t3;
        if constexpr(std::is_unsigned_v<T>) return std::numeric_limits<T>::max();
        else if(t1 < 0) return std::numeric_limits<T>::min();
        else return std::numeric_limits<T>::max();
    }
};

template <typename T>
struct sat_sub {
    constexpr T operator()(const T t1, const T t2) const {
        // from https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/bits/sat_arith.h#L65
        T t3{};
        if(!__builtin_sub_overflow(t1, t2, &t3)) return t3;
        if constexpr(std::is_unsigned_v<T>) return 0;
        else if(t1 < 0) return std::numeric_limits<T>::min();
        else return std::numeric_limits<T>::max();
    }
};

template <typename T> struct min_struct { constexpr T operator()(const T t1, const T t2) const { return std::min(t1, t2); } };
template <typename T> struct max_struct { constexpr T operator()(const T t1, const T t2) const { return std::max(t1, t2); } };
// pseudo-min/max as defined in https://github.com/WebAssembly/simd/blob/main/proposals/simd/SIMD.md#pseudo-minimum
template <typename T> struct p_min_struct { constexpr T operator()(const T t1, const T t2) const { return t2 < t1 ? t2 : t1; } };
template <typename T> struct p_max_struct { constexpr T operator()(const T t1, const T t2) const { return t1 < t2 ? t2 : t1; } };

template <typename T> struct shift_left {
    constexpr T operator()(const T t1, const int32_t t2) const { return static_cast<T>(t1 << (t2 % v128_traits<T>::bits)); }
};
template <typename T> struct arith_shift_right {
    // sign-preserving right shift ~> works only "correctly" on signed types
    constexpr T operator()(const T t1, const int32_t t2) const {
        using type = typename v128_traits<T>::signed_t;
        type st1;

        if constexpr(std::is_same_v<type, T>) st1 = t1;
        else memcpy(&st1, &t1, sizeof(T));

        st1 >>= (t2 % v128_traits<T>::bits);

        T result;
        if constexpr(std::is_same_v<type, T>) result = st1;
        else memcpy(&result, &st1, sizeof(T));

        return result;
    }
};
template <typename T> struct logic_shift_right {
    // zero fill right shift ~> works only "correctly" on unsigned types
    constexpr T operator()(const T t1, const int32_t t2) const {
        using type = typename v128_traits<T>::unsigned_t;
        type ut1;

        if constexpr(std::is_same_v<type, T>) ut1 = t1;
        else memcpy(&ut1, &t1, sizeof(T));

        ut1 >>= (t2 % v128_traits<T>::bits);

        T result;
        if constexpr(std::is_same_v<type, T>) result = ut1;
        else memcpy(&result, &ut1, sizeof(T));

        return result;
    }
};
}

bool i_instr_simd_bin_v128_v128_op(Module* m, uint8_t opcode){
    // need to keep copies!
    const auto v1 = m->stack[m->sp - 1];
    const auto v2 = m->stack[m->sp];
    m->sp--;

    switch(opcode) {
        // i8x16 -> ==, !=, <, <u, >, >u, <=, <=u, >=, >=u
        case 0x23: return simd_bin_op<int8_t, std::equal_to>(m, v1, v2);
        case 0x24: return simd_bin_op<int8_t, std::not_equal_to>(m, v1, v2);
        case 0x25: return simd_bin_op<int8_t, std::less>(m, v1, v2);
        case 0x26: return simd_bin_op<int8_t, std::less, uint8_t>(m, v1, v2);
        case 0x27: return simd_bin_op<int8_t, std::greater>(m, v1, v2);
        case 0x28: return simd_bin_op<int8_t, std::greater, uint8_t>(m, v1, v2);
        case 0x29: return simd_bin_op<int8_t, std::less_equal>(m, v1, v2);
        case 0x2a: return simd_bin_op<int8_t, std::less_equal, uint8_t>(m, v1, v2);
        case 0x2b: return simd_bin_op<int8_t, std::greater_equal>(m, v1, v2);
        case 0x2c: return simd_bin_op<int8_t, std::greater_equal, uint8_t>(m, v1, v2);
        // i16x8 -> ==, !=, <, <u, >, >u, <=, <=u, >=, >=u
        case 0x2d: return simd_bin_op<int16_t, std::equal_to>(m, v1, v2);
        case 0x2e: return simd_bin_op<int16_t, std::not_equal_to>(m, v1, v2);
        case 0x2f: return simd_bin_op<int16_t, std::less>(m, v1, v2);
        case 0x30: return simd_bin_op<int16_t, std::less, uint16_t>(m, v1, v2);
        case 0x31: return simd_bin_op<int16_t, std::greater>(m, v1, v2);
        case 0x32: return simd_bin_op<int16_t, std::greater, uint16_t>(m, v1, v2);
        case 0x33: return simd_bin_op<int16_t, std::less_equal>(m, v1, v2);
        case 0x34: return simd_bin_op<int16_t, std::less_equal, uint16_t>(m, v1, v2);
        case 0x35: return simd_bin_op<int16_t, std::greater_equal>(m, v1, v2);
        case 0x36: return simd_bin_op<int16_t, std::greater_equal, uint16_t>(m, v1, v2);
        // i32x4 -> ==, !=, <, <u, >, >u, <=, <=u, >=, >=u
        case 0x37: return simd_bin_op<int32_t, std::equal_to>(m, v1, v2);
        case 0x38: return simd_bin_op<int32_t, std::not_equal_to>(m, v1, v2);
        case 0x39: return simd_bin_op<int32_t, std::less>(m, v1, v2);
        case 0x3a: return simd_bin_op<int32_t, std::less, uint32_t>(m, v1, v2);
        case 0x3b: return simd_bin_op<int32_t, std::greater>(m, v1, v2);
        case 0x3c: return simd_bin_op<int32_t, std::greater, uint32_t>(m, v1, v2);
        case 0x3d: return simd_bin_op<int32_t, std::less_equal>(m, v1, v2);
        case 0x3e: return simd_bin_op<int32_t, std::less_equal, uint32_t>(m, v1, v2);
        case 0x3f: return simd_bin_op<int32_t, std::greater_equal>(m, v1, v2);
        case 0x40: return simd_bin_op<int32_t, std::greater_equal, uint32_t>(m, v1, v2);
        // f32x4 -> ==, !=, <, >, <=, >=
        case 0x41: return simd_bin_op<float, std::equal_to>(m, v1, v2);
        case 0x42: return simd_bin_op<float, std::not_equal_to>(m, v1, v2);
        case 0x43: return simd_bin_op<float, std::less>(m, v1, v2);
        case 0x44: return simd_bin_op<float, std::greater>(m, v1, v2);
        case 0x45: return simd_bin_op<float, std::less_equal>(m, v1, v2);
        case 0x46: return simd_bin_op<float, std::greater_equal>(m, v1, v2);
        // f64x2 -> ==, !=, <, >, <=, >=
        case 0x47: return simd_bin_op<double, std::equal_to>(m, v1, v2);
        case 0x48: return simd_bin_op<double, std::not_equal_to>(m, v1, v2);
        case 0x49: return simd_bin_op<double, std::less>(m, v1, v2);
        case 0x4a: return simd_bin_op<double, std::greater>(m, v1, v2);
        case 0x4b: return simd_bin_op<double, std::less_equal>(m, v1, v2);
        case 0x4c: return simd_bin_op<double, std::greater_equal>(m, v1, v2);
        // i8x16 -> +, + sat s, + sat u, -, - sat s, - sat u
        case 0x6e: return simd_bin_op<int8_t, std::plus>(m, v1, v2);
        case 0x6f: return simd_bin_op<int8_t, sat_add>(m, v1, v2);
        case 0x70: return simd_bin_op<int8_t, sat_add, uint8_t>(m, v1, v2);
        case 0x71: return simd_bin_op<int8_t, std::minus>(m, v1, v2);
        case 0x72: return simd_bin_op<int8_t, sat_sub>(m, v1, v2);
        case 0x73: return simd_bin_op<int8_t, sat_sub, uint8_t>(m, v1, v2);
        // i8x16 min s, min u, max s, max u (can't pass std::min/std::max as template argument, they are function-overload-sets)
        case 0x76: return simd_bin_op<int8_t, min_struct>(m, v1, v2);
        case 0x77: return simd_bin_op<int8_t, min_struct, uint8_t>(m, v1, v2);
        case 0x78: return simd_bin_op<int8_t, max_struct>(m, v1, v2);
        case 0x79: return simd_bin_op<int8_t, max_struct, uint8_t>(m, v1, v2);
        // i16x8 -> +, + sat s, + sat u, -, - sat s, - sat u, *
        case 0x8e: return simd_bin_op<int16_t, std::plus>(m, v1, v2);
        case 0x8f: return simd_bin_op<int16_t, sat_add>(m, v1, v2);
        case 0x90: return simd_bin_op<int16_t, sat_add, uint16_t>(m, v1, v2);
        case 0x91: return simd_bin_op<int16_t, std::minus>(m, v1, v2);
        case 0x92: return simd_bin_op<int16_t, sat_sub>(m, v1, v2);
        case 0x93: return simd_bin_op<int16_t, sat_sub, uint16_t>(m, v1, v2);
        case 0x95: return simd_bin_op<int16_t, std::multiplies>(m, v1, v2);
        // i16x8 min s, min u, max s, max u
        case 0x96: return simd_bin_op<int16_t, min_struct>(m, v1, v2);
        case 0x97: return simd_bin_op<int16_t, min_struct, uint16_t>(m, v1, v2);
        case 0x98: return simd_bin_op<int16_t, max_struct>(m, v1, v2);
        case 0x99: return simd_bin_op<int16_t, max_struct, uint16_t>(m, v1, v2);
        // i32x4 -> +, -, *
        case 0xae: return simd_bin_op<int32_t, std::plus>(m, v1, v2);
        case 0xb1: return simd_bin_op<int32_t, std::minus>(m, v1, v2);
        case 0xb5: return simd_bin_op<int32_t, std::multiplies>(m, v1, v2);
        // i32x4 min s, min u, max s, max u
        case 0xb6: return simd_bin_op<int32_t, min_struct>(m, v1, v2);
        case 0xb7: return simd_bin_op<int32_t, min_struct, uint32_t>(m, v1, v2);
        case 0xb8: return simd_bin_op<int32_t, max_struct>(m, v1, v2);
        case 0xb9: return simd_bin_op<int32_t, max_struct, uint32_t>(m, v1, v2);
        // i64x2 -> +, -, *
        case 0xce: return simd_bin_op<int64_t, std::plus>(m, v1, v2);
        case 0xd1: return simd_bin_op<int64_t, std::minus>(m, v1, v2);
        case 0xd5: return simd_bin_op<int64_t, std::multiplies>(m, v1, v2);
        // i64x2 -> ==, !=, <, >, <=, >=
        case 0xd6: return simd_bin_op<int64_t, std::equal_to>(m, v1, v2);
        case 0xd7: return simd_bin_op<int64_t, std::not_equal_to>(m, v1, v2);
        case 0xd8: return simd_bin_op<int64_t, std::less>(m, v1, v2);
        case 0xd9: return simd_bin_op<int64_t, std::greater>(m, v1, v2);
        case 0xda: return simd_bin_op<int64_t, std::less_equal>(m, v1, v2);
        case 0xdb: return simd_bin_op<int64_t, std::greater_equal>(m, v1, v2);
        // f32x4 -> +, -, *, /, min, max, pmin, pmax
        case 0xe4: return simd_bin_op<float, std::plus>(m, v1, v2);
        case 0xe5: return simd_bin_op<float, std::minus>(m, v1, v2);
        case 0xe6: return simd_bin_op<float, std::multiplies>(m, v1, v2);
        case 0xe7: return simd_bin_op<float, std::divides>(m, v1, v2);
        case 0xe8: return simd_bin_op<float, min_struct>(m, v1, v2);
        case 0xe9: return simd_bin_op<float, max_struct>(m, v1, v2);
        case 0xea: return simd_bin_op<float, p_min_struct>(m, v1, v2);
        case 0xeb: return simd_bin_op<float, p_max_struct>(m, v1, v2);
        // f64x2 -> +, -, *, /, min, max
        case 0xf0: return simd_bin_op<double, std::plus>(m, v1, v2);
        case 0xf1: return simd_bin_op<double, std::minus>(m, v1, v2);
        case 0xf2: return simd_bin_op<double, std::multiplies>(m, v1, v2);
        case 0xf3: return simd_bin_op<double, std::divides>(m, v1, v2);
        case 0xf4: return simd_bin_op<double, min_struct>(m, v1, v2);
        case 0xf5: return simd_bin_op<double, max_struct>(m, v1, v2);
        case 0xf6: return simd_bin_op<double, p_min_struct>(m, v1, v2);
        case 0xf7: return simd_bin_op<double, p_max_struct>(m, v1, v2);

        default:
            return false;
    }
}

bool i_instr_simd_shift(Module* m, uint8_t opcode){
    // need to keep copies!
    const auto v1 = m->stack[m->sp - 1];
    const auto v2 = m->stack[m->sp];
    m->sp--;

    switch(opcode) {
        // i8x16 <<, >>(s), >>(u)
        case 0x6b: return simd_shift<int8_t, shift_left>(m, v1, v2);
        case 0x6c: return simd_shift<int8_t, arith_shift_right>(m, v1, v2);
        case 0x6d: return simd_shift<int8_t, logic_shift_right>(m, v1, v2);
        // i16x8 <<, >>(s), >>(u)
        case 0x8b: return simd_shift<int16_t, shift_left>(m, v1, v2);
        case 0x8c: return simd_shift<int16_t, arith_shift_right>(m, v1, v2);
        case 0x8d: return simd_shift<int16_t, logic_shift_right>(m, v1, v2);
        // i32x4 <<, >>(s), >>(u)
        case 0xab: return simd_shift<int32_t, shift_left>(m, v1, v2);
        case 0xac: return simd_shift<int32_t, arith_shift_right>(m, v1, v2);
        case 0xad: return simd_shift<int32_t, logic_shift_right>(m, v1, v2);
        // i64x2 <<, >>(s), >>(u)
        case 0xcb: return simd_shift<int64_t, shift_left>(m, v1, v2);
        case 0xcc: return simd_shift<int64_t, arith_shift_right>(m, v1, v2);
        case 0xcd: return simd_shift<int64_t, logic_shift_right>(m, v1, v2);

        default:
            return false;
    }
}

bool i_instr_simd_ext_add_pairwise(Module* m, uint8_t opcode){
    switch(opcode) {
        case 0x7c: return simd_ext_add<int8_t>(m);
        case 0x7d: return simd_ext_add<uint8_t>(m);
        case 0x7e: return simd_ext_add<int16_t>(m);
        case 0x7f: return simd_ext_add<uint16_t>(m);
        default: return false;
    }
}
