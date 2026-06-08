#include "interpreter.h"

#include <cassert>
#include <cmath>
#include <cstring>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/CallbackHandler.h"
#include "interp_loop.h"

//
// Stack machine (byte code related functions)
//

void Interpreter::push_block(Module *m, Block *block, int sp) {
    ExecutionContext *ectx = m->warduino->execution_context;

    ectx->csp += 1;
    ectx->callstack[ectx->csp].block = block;
    ectx->callstack[ectx->csp].sp = sp;
    ectx->callstack[ectx->csp].fp = ectx->fp;
    ectx->callstack[ectx->csp].ra_ptr = ectx->pc_ptr;

    ectx->callstack[ectx->csp].module = m;
}

Block *Interpreter::pop_block(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    Frame *frame = &ectx->callstack[ectx->csp--];
    Type *t = frame->block->type;

    if (frame->block->block_type == 0xff) {
        CallbackHandler::resolving_event = false;
        // free if event guard
        free(frame->block);
        frame->block = nullptr;
        frame = &ectx->callstack[ectx->csp--];
        t = frame->block->type;
    }

    if (frame->block->block_type == 0xfe) {
        m->warduino->program_state = PROXYhalt;
        m->warduino->debugger->sendProxyCallResult(m);
        // free if proxy guard
        free(frame->block);
        frame->block = nullptr;
        frame = &ectx->callstack[ectx->csp--];
        t = frame->block->type;
    }

    // TODO: validate return value if there is one

    ectx->fp = frame->fp;  // Restore frame pointer

    // Validate the return value
    for (uint32_t i = 0; i < t->result_count; i++) {
        if (ectx->stack[ectx->sp - (t->result_count - 1 - i)].value_type !=
            t->results[i]) {
            sprintf(exception, "call type mismatch");
            return nullptr;
        }
    }
    // Restore stack pointer
    if (t->result_count > 0) {
        for (uint32_t i = 0; i < t->result_count; i++) {
            ectx->stack[frame->sp + 1 + i] =
                ectx->stack[ectx->sp - (t->result_count - 1) + i];
        }
        ectx->sp = frame->sp + t->result_count;
    } else {
        if (frame->sp < ectx->sp) {
            ectx->sp = frame->sp;
        }
    }

    if (frame->block->block_type == 0x00) {
        // Function, set pc to return address
        ectx->pc_ptr = frame->ra_ptr;

        if (ectx->csp >= 0) {
            Module *caller_module = ectx->callstack[ectx->csp].module;
            if (caller_module != m) {
                m->warduino->switch_to_module(caller_module);
            }
        }
    }

    return frame->block;
}

void Interpreter::setup_call(Module *m, uint32_t fidx) {
    ExecutionContext *ectx = m->warduino->execution_context;
    Block *func = &m->functions[fidx];
    Type *type = func->type;

    // Push current frame on the call stack
    push_block(m, func, ectx->sp - type->param_count);

#if TRACE
    dbg_warn("  >> fn0x%x(%d) %s(", fidx, fidx,
             func->export_name
                 ? func->export_name
                 : "") for (int p = ((int)type->param_count) - 1; p >= 0; p--) {
        dbg_warn("%s%s", value_repr(&ectx->stack[ectx->sp - p]), p ? " " : "");
    }
    dbg_warn("), %d locals, %d results\n", func->local_count,
             type->result_count);
#endif

    // Push locals (dropping extras)
    ectx->fp = ectx->sp - ((int)type->param_count) + 1;
    // TODO: validate arguments vs formal params

    // Push function locals
    for (uint32_t lidx = 0; lidx < func->local_count; lidx++) {
        ectx->sp += 1;
#if DEBUG || TRACE || WARN || INFO
        if (ectx->sp >= STACK_SIZE) {
            FATAL("WebAssembly stack overflow ectx->sp = %d, STACK_SIZE = %d\n",
                  ectx->sp, STACK_SIZE);
        }
#endif
        memset(&ectx->stack[ectx->sp], 0, sizeof(StackValue));
        ectx->stack[ectx->sp].value_type = func->local_value_type[lidx];
    }

    // Set program counter to start of function
    ectx->pc_ptr = func->start_ptr;

    // Store the module
    ectx->callstack[ectx->csp].module = m;
}

// Size of memory load/store operations indexed by opcode - 0x28
uint32_t LOAD_SIZE[] = {4, 8, 4, 8, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4};
uint32_t LOAD_TYPES[] = {I32, I64, F32, F64, I32, I32, I32,
                         I32, I64, I64, I64, I64, I64, I64};
uint32_t STORE_SIZE[] = {4, 8, 4, 8, 1, 2, 1, 2, 4};

void Interpreter::load(Module *m, uint32_t offset, uint32_t addr, int size,
                       uint8_t value_type, bool sign_extend) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint8_t *maddr = m->memory.bytes + offset + addr;
    ectx->stack[ectx->sp].value.uint64 = 0;  // initialize to 0
    memcpy(&ectx->stack[ectx->sp].value, maddr, size);
    ectx->stack[ectx->sp].value_type = value_type;
}

void Interpreter::store(Module *m, uint32_t offset, uint32_t addr,
                        int value_sp, int size) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint8_t *maddr = m->memory.bytes + offset + addr;
    memcpy(maddr, &ectx->stack[value_sp].value, size);
}

void Interpreter::report_overflow([[maybe_unused]] Module *m,
                                  [[maybe_unused]] uint8_t *maddr) {
    dbg_warn("memory start: %p, memory end: %p, maddr: %p\n", m->memory.bytes,
             m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE, maddr);
    sprintf(exception, "out of bounds memory access");
}

// performs proxy calls to an MCU
bool proxy_call(Module *m, uint32_t fidx) {
    dbg_info("Remote Function Call %d\n", fidx);
    ExecutionContext *ectx = m->warduino->execution_context;
    ProxySupervisor *supervisor = m->warduino->debugger->supervisor;
    RFC *rfc;
    Type *type = m->functions[fidx].type;
    if (type->param_count > 0) {
        ectx->sp -= type->param_count;
        StackValue *args = &ectx->stack[ectx->sp + 1];
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
        ectx->stack[++ectx->sp] = *rfc->result;
    }
    return true;
}

/*
 * WebAssembly Instructions
 *
 * Interpreter::i_instr_**** member functions
 *
 * Functions here are called from the template dispatch loop in interp_loop.h.
 *
 * The given module `m` is the currently executing module
 *
 * Returning false breaks this loop and marks the execution as crashed.
 * The exception variable can be filled with an explanation.
 *
 * Returning true continues the loop with the notable exception of end
 * which can terminate the program successfully by setting program_done
 */

/**
 * 0x02
 */
bool Interpreter::i_instr_block(Module *m, uint8_t *block_ptr) {
    ExecutionContext *ectx = m->warduino->execution_context;
    read_LEB_32(&ectx->pc_ptr);  // ignore block type
    if (ectx->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    auto block_itr = m->block_lookup.find(block_ptr);
    ASSERT(block_itr != m->block_lookup.end(), "could not find block");
    push_block(m, block_itr->second, ectx->sp);
    return true;
}

/**
 * 0x03
 */
bool Interpreter::i_instr_loop(Module *m, uint8_t *block_ptr) {
    ExecutionContext *ectx = m->warduino->execution_context;
    read_LEB_32(&ectx->pc_ptr);  // ignore block type
    if (ectx->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    push_block(m, m->block_lookup[block_ptr], ectx->sp);
    return true;
}

/**
 * 0x04 if
 */
bool Interpreter::i_instr_if(Module *m, uint8_t *block_ptr) {
    ExecutionContext *ectx = m->warduino->execution_context;
    read_LEB_32(&ectx->pc_ptr);  // ignore block type
    Block *block = m->block_lookup[block_ptr];

    if (ectx->csp >= CALLSTACK_SIZE) {
        sprintf(exception, "call stack exhausted");
        return false;
    }
    uint32_t cond = ectx->stack[ectx->sp--].value.uint32;
    push_block(m, block, ectx->sp);

    if (cond == 0) {  // if false (I32)
        // branch to else block or after end of if
        if (block->else_ptr == nullptr) {
            // no else block, pop if block and skip end
            ectx->csp -= 1;
            ectx->pc_ptr = block->br_ptr + 1;
        } else {
            ectx->pc_ptr = block->else_ptr;
        }
    }
    // if true, keep going
#if TRACE
    debug("      - cond: 0x%x jump to 0x%x, block: %s\n", cond,
          (uint32_t)(ectx->pc_ptr - m->bytes), block_repr(block));
#endif
    return true;
}

/**
 * 0x05 else
 */
bool Interpreter::i_instr_else(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    Block *block = ectx->callstack[ectx->csp].block;
    ectx->pc_ptr = block->br_ptr;
#if TRACE
    debug("      - of %s jump to 0x%p\n", block_repr(block), ectx->pc_ptr);
#endif
    return true;
}

/**
 * 0x0b end
 */
bool Interpreter::i_instr_end(Module *m, bool *prog_done) {
    ExecutionContext *ectx = m->warduino->execution_context;
    Block *block = pop_block(m);
    if (block == nullptr) {
        return false;  // an exception (set by pop_block)
    }
#if TRACE
    debug("      - of %s\n", block_repr(block));
#endif
    if (block->block_type == 0x00) {  // Function
#if TRACE
        dbg_warn("  << fn0x%x(%d) %s = %s\n", block->fidx, block->fidx,
                 block->export_name ? block->export_name : "",
                 block->type->result_count > 0
                     ? value_repr(&ectx->stack[ectx->sp])
                     : "_");
#endif
        if (ectx->csp == -1) {
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
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t depth = read_LEB_32(&ectx->pc_ptr);
    ectx->csp -= depth;
    // set to end for pop_block
    ectx->pc_ptr = ectx->callstack[ectx->csp].block->br_ptr;
#if TRACE
    debug("      - to: 0x%p\n", ectx->pc_ptr);
#endif
    return true;
}

/**
 * 0x0d br_if
 */
bool Interpreter::i_instr_br_if(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t depth = read_LEB_32(&ectx->pc_ptr);

    uint32_t cond = ectx->stack[ectx->sp--].value.uint32;
    if (cond) {  // if true
        ectx->csp -= depth;
        // set to end for pop_block
        ectx->pc_ptr = ectx->callstack[ectx->csp].block->br_ptr;
    }
#if TRACE
    debug("      - depth: 0x%x, cond: 0x%x, to: 0x%p\n", depth, cond,
          ectx->pc_ptr);
#endif
    return true;
}

/**
 * 0x0e br_table
 */
bool Interpreter::i_instr_br_table(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t count = read_LEB_32(&ectx->pc_ptr);
    if (count > BR_TABLE_SIZE) {
        // TODO: check this prior to runtime
        sprintf(exception, "br_table size %" PRIu32 " exceeds max %d\n", count,
                BR_TABLE_SIZE);
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        ectx->br_table[i] = read_LEB_32(&ectx->pc_ptr);
    }
    uint32_t depth = read_LEB_32(&ectx->pc_ptr);

    int32_t didx = ectx->stack[ectx->sp--].value.int32;
    if (didx >= 0 && didx < (int32_t)count) {
        depth = ectx->br_table[didx];
    }

    ectx->csp -= depth;
    // set to end for pop_block
    ectx->pc_ptr = ectx->callstack[ectx->csp].block->br_ptr;
#if TRACE
    debug("      - count: %d, didx: %d, to: 0x%p\n", count, didx,
          ectx->pc_ptr);
#endif
    return true;
}

/**
 * 0x0f return
 */
bool Interpreter::i_instr_return(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    while (ectx->csp >= 0 &&
           ectx->callstack[ectx->csp].block->block_type != 0x00) {
        ectx->csp--;
    }
    // Set the program count to the end of the function
    // The actual pop_block and return is handled by the end opcode.
    ectx->pc_ptr = ectx->callstack[0].block->end_ptr;
#if TRACE
    debug("      - to: 0x%p\n", ectx->pc_ptr);
#endif
    return true;
}

/**
 * 0x10 call
 */
bool Interpreter::i_instr_call(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t fidx = read_LEB_32(&ectx->pc_ptr);

    if (m->warduino->debugger->isProxied(fidx)) {
        return proxy_call(m, fidx);
    }

    if (fidx < m->import_count) {
        Block *func = &m->functions[fidx];

        // Mocking only works on primitives, no need to check for it otherwise.
        if (ectx->sp >= 0) {
            uint32_t arg = ectx->stack[ectx->sp].value.uint32;
            if (m->warduino->debugger->isMocked(fidx, arg)) {
                ectx->stack[ectx->sp].value.uint32 =
                    m->warduino->debugger->getMockedValue(fidx, arg);
                return true;
            }
        }

        if (func->import_module != nullptr &&
            strcmp(func->import_module, "env") != 0) {
            // Cross-module call!
            Module *target = m->warduino->get_module(func->import_module);
            if (target != nullptr) {
                // Find the exported function in target module
                uint32_t target_fidx =
                    m->warduino->get_export_fidx(target, func->import_field);
                if (target_fidx != (uint32_t)-1) {
                    // Switch to target module and call
                    m->warduino->switch_to_module(target);
                    setup_call(target, target_fidx);
                    return true;
                }
            } else {
                sprintf(exception, "import module %s not found",
                        func->import_module);
                return false;
            }
        }

        return ((Primitive)m->functions[fidx].func_ptr)(m);
    } else {
        if (ectx->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            return false;
        }
        setup_call(m, fidx);  // regular function call
#if TRACE
        debug("      - calling function fidx: %d at: 0x%p\n", fidx,
              ectx->pc_ptr);
#endif
    }
    return true;
}

/**
 * 0x11 call_indirect
 */
bool Interpreter::i_instr_call_indirect(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t tidx = read_LEB_32(&ectx->pc_ptr);
    read_LEB_32(&ectx->pc_ptr);  // reserved immediate
    uint32_t val = ectx->stack[ectx->sp--].value.uint32;
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

    Module *target_module = m;
    if (m->table.imported && m->table.owner != nullptr) {
        target_module = m->table.owner;
    }
#if TRACE
    debug("       - call_indirect tidx: %d, val: 0x%x, fidx: 0x%x\n", tidx,
          val, fidx);
#endif

    if (fidx < target_module->import_count) {
        // THUNK thunk_out(m, fidx);    // import/thunk call
    } else {
        Block *func = &target_module->functions[fidx];
        Type *ftype = func->type;

        if (ectx->csp >= CALLSTACK_SIZE) {
            sprintf(exception, "call stack exhausted");
            return false;
        }
        if (ftype->mask != m->types[tidx].mask) {
            sprintf(exception,
                    "indirect call type mismatch (call type and function type "
                    "differ)");
            return false;
        }

        if (target_module != m) {
            m->warduino->switch_to_module(target_module);
        }

        setup_call(target_module, fidx);  // regular function call

        // Validate signatures match
        if ((int)(ftype->param_count + func->local_count) !=
            ectx->sp - ectx->fp + 1) {
            sprintf(exception,
                    "indirect call type mismatch (param counts differ)");
            return false;
        }
        for (uint32_t f = 0; f < ftype->param_count; f++) {
            if (ftype->params[f] != ectx->stack[ectx->fp + f].value_type) {
                sprintf(exception,
                        "indirect call type mismatch (param types differ)");
                return false;
            }
        }

#if TRACE
        debug(
            "      - tidx: %d, table idx: %d, "
            "calling function fidx: %d at: 0x%p\n",
            tidx, val, fidx, ectx->pc_ptr);
#endif
    }
    return true;
}

/**
 * 0x1a drop
 * remove a value from the stack
 */
bool Interpreter::i_instr_drop(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    ectx->sp--;
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
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t cond = ectx->stack[ectx->sp--].value.uint32;
    ectx->sp--;
    if (!cond) {  // use a instead of b
        ectx->stack[ectx->sp] = ectx->stack[ectx->sp + 1];
    }
    return true;
}

/**
 * 0x20 get_local
 * move the i-th local to the top of the stack
 */
bool Interpreter::i_instr_get_local(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    int32_t arg = read_LEB_32(&ectx->pc_ptr);
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg,
          value_repr(&ectx->stack[ectx->fp + arg]));
#endif
    ectx->stack[++ectx->sp] = ectx->stack[ectx->fp + arg];
    return true;
}

/**
 * 0x21 set_local
 */
bool Interpreter::i_instr_set_local(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    int32_t arg = read_LEB_32(&ectx->pc_ptr);
    ectx->stack[ectx->fp + arg] = ectx->stack[ectx->sp--];
#if TRACE
    debug("      - arg: 0x%x, to %s (stack loc: %d)\n", arg,
          value_repr(&ectx->stack[ectx->sp + 1]), ectx->fp + arg);
#endif
    return true;
}

/**
 * 0x22 tee_local
 */
bool Interpreter::i_instr_tee_local(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    int32_t arg = read_LEB_32(&ectx->pc_ptr);
    ectx->stack[ectx->fp + arg] = ectx->stack[ectx->sp];
#if TRACE
    debug("      - arg: 0x%x, to %s\n", arg,
          value_repr(&ectx->stack[ectx->sp]));
#endif
    return true;
}

/**
 * 0x23 get_global
 */
bool Interpreter::i_instr_get_global(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    int32_t arg = read_LEB_32(&ectx->pc_ptr);
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg,
          value_repr(m->globals[arg]->value));
#endif
    ectx->stack[++ectx->sp] = *m->globals[arg]->value;
    return true;
}

/**
 * 0x24 set_global
 */
bool Interpreter::i_instr_set_global(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t arg = read_LEB_32(&ectx->pc_ptr);
    *m->globals[arg]->value = ectx->stack[ectx->sp--];
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg,
          value_repr(&ectx->stack[ectx->sp + 1]));
#endif
    return true;
}

/**
 * 0x3f current_memory
 */
bool Interpreter::i_instr_current_memory(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    read_LEB_32(&ectx->pc_ptr);  // ignore reserved
    ectx->stack[++ectx->sp].value_type = I32;
    ectx->stack[ectx->sp].value.uint32 = m->memory.pages;
    return true;
}

/**
 * 0x40 grow_memory
 */
bool Interpreter::i_instr_grow_memory(Module *m) {
    ExecutionContext *ectx = m->warduino->execution_context;
    read_LEB_32(&ectx->pc_ptr);  // ignore reserved
    uint32_t prev_pages = m->memory.pages;
    uint32_t delta = ectx->stack[ectx->sp].value.uint32;
    ectx->stack[ectx->sp].value.uint32 = prev_pages;
    if (delta == 0) {
        return true;  // No change
    } else if (delta + prev_pages > m->memory.maximum) {
        ectx->stack[ectx->sp].value.uint32 = static_cast<uint32_t>(-1);
        return true;
    }
    m->memory.pages += delta;
    m->memory.bytes = (uint8_t *)arecalloc(
        m->memory.bytes, prev_pages * PAGE_SIZE, m->memory.pages * PAGE_SIZE,
        1 /*sizeof(uint32_t)*/, "Module->memory.bytes", true);
    return true;
}

/**
 * 0x28 ... 0x35 memory load operators
 */
bool Interpreter::i_instr_mem_load(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t flags = read_LEB_32(&ectx->pc_ptr);
    uint32_t offset = read_LEB_32(&ectx->pc_ptr);
    uint32_t addr = ectx->stack[ectx->sp--].value.uint32;
    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned load - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x\n",
            flags, offset, addr);
    }

    bool overflow = false;
    if (offset + addr < addr) {
        overflow = true;
    }
    uint8_t *maddr = m->memory.bytes + offset + addr;
    if (maddr < m->memory.bytes) {
        overflow = true;
    }
    uint8_t *mem_end =
        m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + LOAD_SIZE[opcode - 0x28] > mem_end) {
        overflow = true;
    }
    dbg_info("      - addr: 0x%x, offset: 0x%x, maddr: %p, mem_end: %p\n",
             addr, offset, maddr, mem_end);
    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            report_overflow(m, maddr);
            return false;
        }
    }

    ectx->stack[++ectx->sp].value.uint64 = 0;  // initialize to 0
    switch (opcode) {
        case 0x28:
            this->load(m, offset, addr, 4, I32, false);
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.load
        case 0x29:
            this->load(m, offset, addr, 8, I64, false);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.load
        case 0x2a:
            this->load(m, offset, addr, 4, F32, false);
            ectx->stack[ectx->sp].value_type = F32;
            break;  // f32.load
        case 0x2b:
            this->load(m, offset, addr, 8, F64, false);
            ectx->stack[ectx->sp].value_type = F64;
            break;  // f64.load
        case 0x2c:
            this->load(m, offset, addr, 1, I32, true);
            sext_8_32(&ectx->stack[ectx->sp].value.uint32);
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.load8_s
        case 0x2d:
            this->load(m, offset, addr, 1, I32, false);
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.load8_u
        case 0x2e:
            this->load(m, offset, addr, 2, I32, true);
            sext_16_32(&ectx->stack[ectx->sp].value.uint32);
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.load16_s
        case 0x2f:
            this->load(m, offset, addr, 2, I32, false);
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.load16_u
        case 0x30:
            this->load(m, offset, addr, 1, I64, true);
            sext_8_64(&ectx->stack[ectx->sp].value.uint64);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.load8_s
        case 0x31:
            this->load(m, offset, addr, 1, I64, false);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.load8_u
        case 0x32:
            this->load(m, offset, addr, 2, I64, true);
            sext_16_64(&ectx->stack[ectx->sp].value.uint64);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.load16_s
        case 0x33:
            this->load(m, offset, addr, 2, I64, false);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.load16_u
        case 0x34:
            this->load(m, offset, addr, 4, I64, true);
            sext_32_64(&ectx->stack[ectx->sp].value.uint64);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.load32_s
        case 0x35:
            this->load(m, offset, addr, 4, I64, false);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.load32_u
        default:
            return false;
    }
    return true;
}

/**
 * 0x36 ... 0x3e memory store operators
 */
bool Interpreter::i_instr_mem_store(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t flags = read_LEB_32(&ectx->pc_ptr);
    uint32_t offset = read_LEB_32(&ectx->pc_ptr);
    int value_sp = ectx->sp--;  // position of the data we want to store
    uint32_t addr = ectx->stack[ectx->sp--].value.uint32;
    bool overflow = false;

    if (flags != 2 && TRACE) {
        dbg_info(
            "      - unaligned store - flags: 0x%x,"
            " offset: 0x%x, addr: 0x%x\n",
            flags, offset, addr);
    }

    if (offset + addr < addr) {
        overflow = true;
    }
    uint8_t *maddr = m->memory.bytes + offset + addr;
    if (maddr < m->memory.bytes) {
        overflow = true;
    }
    uint8_t *mem_end =
        m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + LOAD_SIZE[opcode - 0x28] > mem_end) {
        overflow = true;
    }
    dbg_info(
        "      - addr: 0x%x, offset: 0x%x, maddr: %p, mem_end: %p\n",
        addr, offset, maddr, mem_end);
    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            report_overflow(m, maddr);
            return false;
        }
    }

    switch (opcode) {
        case 0x36:
            this->store(m, offset, addr, value_sp, 4);
            break;  // i32.store
        case 0x37:
            this->store(m, offset, addr, value_sp, 8);
            break;  // i64.store
        case 0x38:
            this->store(m, offset, addr, value_sp, 4);
            break;  // f32.store
        case 0x39:
            this->store(m, offset, addr, value_sp, 8);
            break;  // f64.store
        case 0x3a:
            this->store(m, offset, addr, value_sp, 1);
            break;  // i32.store8
        case 0x3b:
            this->store(m, offset, addr, value_sp, 2);
            break;  // i32.store16
        case 0x3c:
            this->store(m, offset, addr, value_sp, 1);
            break;  // i64.store8
        case 0x3d:
            this->store(m, offset, addr, value_sp, 2);
            break;  // i64.store16
        case 0x3e:
            this->store(m, offset, addr, value_sp, 4);
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
    ExecutionContext *ectx = m->warduino->execution_context;
    StackValue *target = &ectx->stack[++ectx->sp];

    switch (opcode) {
        case 0x41:  // i32.const
            target->value_type = I32;
            target->value.uint32 = read_LEB_signed(&ectx->pc_ptr, 32);
            break;
        case 0x42:  // i64.const
            target->value_type = I64;
            target->value.int64 = read_LEB_signed(&ectx->pc_ptr, 64);
            break;
        case 0x43:  // f32.const
            target->value_type = F32;
            memcpy(&target->value.uint32, ectx->pc_ptr, 4);
            ectx->pc_ptr += 4;
            break;
        case 0x44:  // f64.const
            target->value_type = F64;
            memcpy(&target->value.uint64, ectx->pc_ptr, 8);
            ectx->pc_ptr += 8;
            break;
        default:
            return false;
    }
    return true;
}

/**
 * 0x45, 0x50 eqz
 */
bool Interpreter::i_instr_unary_u32(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    switch (opcode) {
        case 0x45:  // i32.eqz
            ectx->stack[ectx->sp].value.uint32 =
                static_cast<uint32_t>(ectx->stack[ectx->sp].value.uint32 == 0);
            break;
        case 0x50:  // i64.eqz
            ectx->stack[ectx->sp].value_type = I32;
            ectx->stack[ectx->sp].value.uint32 =
                static_cast<uint32_t>(ectx->stack[ectx->sp].value.uint64 == 0);
            break;
        default:
            return false;
    }
    return true;
}

/**
 * 0x46...0x4f i32 comparison
 */
bool Interpreter::i_instr_math_u32(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t a = ectx->stack[ectx->sp - 1].value.uint32;
    uint32_t b = ectx->stack[ectx->sp].value.uint32;
    uint32_t c;
    ectx->sp -= 1;
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
    ectx->stack[ectx->sp].value_type = I32;
    ectx->stack[ectx->sp].value.uint32 = c;
    return true;
}

/**
 * 0x51...0x5a i64 comparison
 */
bool Interpreter::i_instr_math_u64(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint64_t d = ectx->stack[ectx->sp - 1].value.uint64;
    uint64_t e = ectx->stack[ectx->sp].value.uint64;
    uint32_t c;
    ectx->sp -= 1;
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
    ectx->stack[ectx->sp].value_type = I32;
    ectx->stack[ectx->sp].value.uint32 = c;
    return true;
}

/**
 * 0x5b...0x60 f32 comparison
 */
bool Interpreter::i_instr_math_f32(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    float g = ectx->stack[ectx->sp - 1].value.f32;
    float h = ectx->stack[ectx->sp].value.f32;
    uint32_t c;
    ectx->sp -= 1;
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
    ectx->stack[ectx->sp].value_type = I32;
    ectx->stack[ectx->sp].value.uint32 = c;
    return true;
}

/**
 * 0x61...0x66 f64 comparison
 */
bool Interpreter::i_instr_math_f64(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    double j = ectx->stack[ectx->sp - 1].value.f64;
    double k = ectx->stack[ectx->sp].value.f64;

    uint32_t c;
    ectx->sp -= 1;
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
    ectx->stack[ectx->sp].value_type = I32;
    ectx->stack[ectx->sp].value.uint32 = c;
    return true;
}

/**
 * 0x67...0x69 unary i32
 */
bool Interpreter::i_instr_unary_i32(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t a = ectx->stack[ectx->sp].value.uint32;
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
    ectx->stack[ectx->sp].value.uint32 = c;
    return true;
}

/**
 * 0x79...0x7b unary i64
 */
bool Interpreter::i_instr_unary_i64(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint64_t d = ectx->stack[ectx->sp].value.uint64;
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
    ectx->stack[ectx->sp].value.uint64 = f;
    return true;
}

/**
 * 0x8b...0x91, 0x99...0x9f unary floating point
 */
bool Interpreter::i_instr_unary_floating(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    switch (opcode) {
        // unary f32
        case 0x8b:
            ectx->stack[ectx->sp].value.f32 =
                fabs(ectx->stack[ectx->sp].value.f32);
            break;  // f32.abs
        case 0x8c:
            ectx->stack[ectx->sp].value.f32 = -ectx->stack[ectx->sp].value.f32;
            break;  // f32.neg
        case 0x8d:
            ectx->stack[ectx->sp].value.f32 =
                ceil(ectx->stack[ectx->sp].value.f32);
            break;  // f32.ceil
        case 0x8e:
            ectx->stack[ectx->sp].value.f32 =
                floor(ectx->stack[ectx->sp].value.f32);
            break;  // f32.floor
        case 0x8f:
            ectx->stack[ectx->sp].value.f32 =
                trunc(ectx->stack[ectx->sp].value.f32);
            break;  // f32.trunc
        case 0x90:
            ectx->stack[ectx->sp].value.f32 =
                rint(ectx->stack[ectx->sp].value.f32);
            break;  // f32.nearest
        case 0x91:
            ectx->stack[ectx->sp].value.f32 =
                sqrt(ectx->stack[ectx->sp].value.f32);
            break;  // f32.sqrt

            // unary f64
        case 0x99:
            ectx->stack[ectx->sp].value.f64 =
                fabs(ectx->stack[ectx->sp].value.f64);
            break;  // f64.abs
        case 0x9a:
            ectx->stack[ectx->sp].value.f64 = -ectx->stack[ectx->sp].value.f64;
            break;  // f64.neg
        case 0x9b:
            ectx->stack[ectx->sp].value.f64 =
                ceil(ectx->stack[ectx->sp].value.f64);
            break;  // f64.ceil
        case 0x9c:
            ectx->stack[ectx->sp].value.f64 =
                floor(ectx->stack[ectx->sp].value.f64);
            break;  // f64.floor
        case 0x9d:
            ectx->stack[ectx->sp].value.f64 =
                trunc(ectx->stack[ectx->sp].value.f64);
            break;  // f64.trunc
        case 0x9e:
            ectx->stack[ectx->sp].value.f64 =
                rint(ectx->stack[ectx->sp].value.f64);
            break;  // f64.nearest
        case 0x9f:
            ectx->stack[ectx->sp].value.f64 =
                sqrt(ectx->stack[ectx->sp].value.f64);
            break;  // f64.sqrt
        default:
            return false;
    }
    return true;
}

/**
 * 0x6a...0x78 binary i32
 */
bool Interpreter::i_instr_binary_i32(Module *m, uint8_t opcode) {
    // TODO: verify if this should not be done with int32_t instead
    ExecutionContext *ectx = m->warduino->execution_context;
    uint32_t a = ectx->stack[ectx->sp - 1].value.uint32;
    uint32_t b = ectx->stack[ectx->sp].value.uint32;
    uint32_t c;
    ectx->sp -= 1;
    if (opcode >= 0x6d && opcode <= 0x70 && b == 0) {
        sprintf(exception, "integer divide by zero");
        return false;
    }
    switch (opcode) {
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
    ectx->stack[ectx->sp].value.uint32 = c;
    return true;
}

/**
 * 0x7c...0x8a binary i64
 */
bool Interpreter::i_instr_binary_i64(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    uint64_t d = ectx->stack[ectx->sp - 1].value.uint64;
    uint64_t e = ectx->stack[ectx->sp].value.uint64;
    uint64_t f;
    ectx->sp -= 1;
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
    ectx->stack[ectx->sp].value.uint64 = f;

    return true;
}

/**
 * 0x92...0x98 binary f32
 */
bool Interpreter::i_instr_binary_f32(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    float g = ectx->stack[ectx->sp - 1].value.f32;
    float h = ectx->stack[ectx->sp].value.f32;
    float i;
    ectx->sp -= 1;
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
    ectx->stack[ectx->sp].value.f32 = i;
    return true;
}

/**
 * 0xa0...0xa6 binary f64
 */
bool Interpreter::i_instr_binary_f64(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    double j = ectx->stack[ectx->sp - 1].value.f64;
    double k = ectx->stack[ectx->sp].value.f64;
    double l;
    ectx->sp -= 1;
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
    ectx->stack[ectx->sp].value.f64 = l;

    return true;
}

/**
 * 0xa7...0xbf conversion operations
 */
bool Interpreter::i_instr_conversion(Module *m, uint8_t opcode) {
    ExecutionContext *ectx = m->warduino->execution_context;
    switch (opcode) {
        case 0xa7:
            ectx->stack[ectx->sp].value.uint64 &= 0x00000000ffffffff;
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.wrap/i64
        case 0xa8:
            if (std::isnan(ectx->stack[ectx->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f32 >= INT32_MAX ||
                       ectx->stack[ectx->sp].value.f32 < INT32_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.int32 = ectx->stack[ectx->sp].value.f32;
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.trunc_s/f32
        case 0xa9:
            if (std::isnan(ectx->stack[ectx->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f32 >= UINT32_MAX ||
                       ectx->stack[ectx->sp].value.f32 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.uint32 =
                ectx->stack[ectx->sp].value.f32;
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.trunc_u/f32
        case 0xaa:
            if (std::isnan(ectx->stack[ectx->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f64 > INT32_MAX ||
                       ectx->stack[ectx->sp].value.f64 < INT32_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.int32 = ectx->stack[ectx->sp].value.f64;
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.trunc_s/f64
        case 0xab:
            if (std::isnan(ectx->stack[ectx->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f64 > UINT32_MAX ||
                       ectx->stack[ectx->sp].value.f64 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.uint32 =
                ectx->stack[ectx->sp].value.f64;
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.trunc_u/f64
        case 0xac:
            ectx->stack[ectx->sp].value.uint64 =
                ectx->stack[ectx->sp].value.uint32;
            sext_32_64(&ectx->stack[ectx->sp].value.uint64);
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.extend_s/i32
        case 0xad:
            ectx->stack[ectx->sp].value.uint64 =
                ectx->stack[ectx->sp].value.uint32;
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.extend_u/i32
        case 0xae:
            if (std::isnan(ectx->stack[ectx->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f32 >= INT64_MAX ||
                       ectx->stack[ectx->sp].value.f32 < INT64_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.int64 = ectx->stack[ectx->sp].value.f32;
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.trunc_s/f32
        case 0xaf:
            if (std::isnan(ectx->stack[ectx->sp].value.f32)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f32 >= UINT64_MAX ||
                       ectx->stack[ectx->sp].value.f32 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.uint64 =
                ectx->stack[ectx->sp].value.f32;
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.trunc_u/f32
        case 0xb0:
            if (std::isnan(ectx->stack[ectx->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f64 >= INT64_MAX ||
                       ectx->stack[ectx->sp].value.f64 < INT64_MIN) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.int64 = ectx->stack[ectx->sp].value.f64;
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.trunc_s/f64
        case 0xb1:
            if (std::isnan(ectx->stack[ectx->sp].value.f64)) {
                sprintf(exception, "invalid conversion to integer");
                return false;
            } else if (ectx->stack[ectx->sp].value.f64 >= UINT64_MAX ||
                       ectx->stack[ectx->sp].value.f64 <= -1) {
                sprintf(exception, "integer overflow");
                return false;
            }
            ectx->stack[ectx->sp].value.uint64 =
                ectx->stack[ectx->sp].value.f64;
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.trunc_u/f64
        case 0xb2:
            ectx->stack[ectx->sp].value.f32 = ectx->stack[ectx->sp].value.int32;
            ectx->stack[ectx->sp].value_type = F32;
            break;  // f32.convert_s/i32
        case 0xb3:
            ectx->stack[ectx->sp].value.f32 =
                ectx->stack[ectx->sp].value.uint32;
            ectx->stack[ectx->sp].value_type = F32;
            break;  // f32.convert_u/i32
        case 0xb4:
            ectx->stack[ectx->sp].value.f32 = ectx->stack[ectx->sp].value.int64;
            ectx->stack[ectx->sp].value_type = F32;
            break;  // f32.convert_s/i64
        case 0xb5:
            ectx->stack[ectx->sp].value.f32 =
                ectx->stack[ectx->sp].value.uint64;
            ectx->stack[ectx->sp].value_type = F32;
            break;  // f32.convert_u/i64
        case 0xb6:
            ectx->stack[ectx->sp].value.f32 =
                (float)ectx->stack[ectx->sp].value.f64;
            ectx->stack[ectx->sp].value_type = F32;
            break;  // f32.demote/f64
        case 0xb7:
            ectx->stack[ectx->sp].value.f64 = ectx->stack[ectx->sp].value.int32;
            ectx->stack[ectx->sp].value_type = F64;
            break;  // f64.convert_s/i32
        case 0xb8:
            ectx->stack[ectx->sp].value.f64 =
                ectx->stack[ectx->sp].value.uint32;
            ectx->stack[ectx->sp].value_type = F64;
            break;  // f64.convert_u/i32
        case 0xb9:
            ectx->stack[ectx->sp].value.f64 = ectx->stack[ectx->sp].value.int64;
            ectx->stack[ectx->sp].value_type = F64;
            break;  // f64.convert_s/i64
        case 0xba:
            ectx->stack[ectx->sp].value.f64 =
                ectx->stack[ectx->sp].value.uint64;
            ectx->stack[ectx->sp].value_type = F64;
            break;  // f64.convert_u/i64
        case 0xbb:
            ectx->stack[ectx->sp].value.f64 = ectx->stack[ectx->sp].value.f32;
            ectx->stack[ectx->sp].value_type = F64;
            break;  // f64.promote/f32

            // reinterpretations
        case 0xbc:
            ectx->stack[ectx->sp].value_type = I32;
            break;  // i32.reinterpret/f32
        case 0xbd:
            ectx->stack[ectx->sp].value_type = I64;
            break;  // i64.reinterpret/f64
        case 0xbe:
            ectx->stack[ectx->sp].value_type = F32;
            break;  // f32.reinterpret/i32
        case 0xbf:
            ectx->stack[ectx->sp].value_type = F64;
            break;  // f64.reinterpret/i64
        default:
            return false;
    }

    return true;
}

/**
 * 0xe0 ... 0xe3 callback operations
 */
// TODO: Remove [[maybe_unused]] when implemented
bool Interpreter::i_instr_callback([[maybe_unused]] Module *m,
                                   [[maybe_unused]] uint8_t opcode) {
    // TODO
    return true;
}

bool Interpreter::interpret(Module *m, bool waiting) {
    return interp<Interpreter>(m, waiting);
}
