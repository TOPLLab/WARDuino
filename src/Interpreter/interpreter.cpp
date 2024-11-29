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

bool Interpreter::interpret(Module *m, bool waiting) {
    uint8_t *block_ptr;
    uint8_t opcode;

    // keep track of occurring errors
    bool success = true;

    // set to true when finishes successfully
    bool program_done = false;

    while ((!program_done && success) || waiting) {
        if (m->warduino->program_state == WARDUINOstep) {
            m->warduino->debugger->notifyCompleteStep(m);
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

        if (m->warduino->debugger->handleContinueFor(m)) {
            continue;
        }

        // Take snapshot before executing an instruction
        if (m->warduino->program_state != WARDUINOinit) {
            m->warduino->debugger->handleSnapshotPolicy(m);
        }

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

            case 0xfd:
                success &= interpret_simd(m);
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
    while (CallbackHandler::resolving_event &&
           CallbackHandler::resolve_event());

    dbg_trace("Interpretation ended %s with status %s\n",
              program_done ? "expectedly" : "unexpectedly",
              success ? "ok" : "error");
    if (!success && m->options.return_exception) {
        m->exception = strdup(exception);
    } else if (!success) {
        FATAL("%s (0x%x)\n", exception, opcode);
    }

    return success;
}

bool Interpreter::interpret_simd(Module *m) {
    // this function should only be called from Interpreter::interpret
    // and should only be called when the opcode is 0xfd
    // -> at this point, the PC is pointing to the (first byte of) the actual
    // opcode

    // TODO: technically the 2nd byte (and onwards) of the multi-byte SIMD
    //       opcodes are LEB-encoded u32's and could have multiple
    //       representations. However, we only support the shortest possible
    //       encodings; see https://webassembly.github.io/spec/core/appendix/index-instructions.html

    constexpr static auto next_pc_0x01 = [](Module *m, const uint8_t opcode) -> bool {
        const auto next = *m->pc_ptr;
        m->pc_ptr++;
        if(next != 0x01) {
            sprintf(exception, "SIMD opcode 0x%02x%02x should be followed by 0x%02x, but got 0x%02x",
                0x7d, opcode, 0x01, next);
            return false;
        }
        return true;
    };

    // TODO: should be removed one day...
    constexpr static auto not_implemented = [](const uint8_t opcode) -> bool {
        sprintf(exception, "SIMD opcode 0x%02x%02x is not implemented (yet)",
            0x7d, opcode);
        return false;
    };

    const auto opcode = *m->pc_ptr;
    m->pc_ptr++;

    switch(opcode) {
        case 0x00: return i_instr_simd_load(m);

        case 0x0b: return i_instr_simd_store(m);
        case 0x0c: return i_instr_simd_const(m);

        case 0x0d: return not_implemented(opcode); // TODO: i8x16.shuffle
        case 0x0e: return i_instr_simd_swizzle(m);

        case 0x0f ... 0x14: return i_instr_simd_splat(m, opcode);

        case 0x15: // interspersed with (dim).extract_lane
        case 0x16:
        case 0x18:
        case 0x19:
        case 0x1b:
        case 0x1d:
        case 0x1f:
        case 0x21:
            return i_instr_simd_extract(m, opcode);

        case 0x17:
        case 0x1a:
        case 0x1c:
        case 0x1e:
        case 0x20:
        case 0x22:
            return i_instr_simd_replace(m, opcode);

        case 0x23 ... 0x2c: // i8x16 relational operators
        case 0x2d ... 0x36: // i16x8 relational operators
        case 0x37 ... 0x40: // i32x4 relational operators
        case 0x41 ... 0x46: // f32x4 relational operators
        case 0x47 ... 0x4c: // f64x2 relational operators
            return i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0x4d: // v128.not
            return i_instr_simd_v128_not(m);

        case 0x4e ... 0x51: // v128 bit-wise operators
            return i_instr_simd_bin_bit_op(m, opcode);

        case 0x52: // TODO -> v128.bitselect
            return not_implemented(opcode);

        case 0x53: // TODO -> v128.any_true
            return not_implemented(opcode);

        case 0x54 ... 0x5d: // TODO -> v128.loadX_lane, v128.storeX_lane, v128.loadX_zero
            return not_implemented(opcode);

        case 0x5e ... 0x5f: // TODO -> demote/promote f32<->f64
            return not_implemented(opcode);

        case 0x60 ... 0x62: // TODO -> i8x16. ~ abs, neg, popcnt
            return not_implemented(opcode);

        case 0x63 ... 0x64: // TODO -> i8x16. ~ all_true, bitmask
            return not_implemented(opcode);

        case 0x65 ... 0x66: // TODO -> i8x16.narrow_i16x8_(s/u)
            return not_implemented(opcode);

        case 0x67 ... 0x6a: // TODO -> f32x4. ~ ceil, floor, trunc, nearest
            return not_implemented(opcode);

        case 0x6b ... 0x6d: // i8x16 shifts
            return i_instr_simd_shift(m, opcode);

        case 0x6e ... 0x73: // i8x16.(add/sub)
            return i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0x74 ... 0x75: // TODO -> f64x2. ~ ceil, floor
            return not_implemented(opcode);

        case 0x76 ... 0x79: // i8x16.(min/max)
            return i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0x7a: // TODO -> f64x2.trunc
            return not_implemented(opcode);

        case 0x7b: // TODO -> i8x16.avgr_u
            return not_implemented(opcode);

        case 0x7c ... 0x7f: // TODO -> (dim).extadd_pairwise_(dim2)
            return i_instr_simd_ext_add_pairwise(m, opcode);

        // --- From 0x80: 3-byte SIMD opcodes, so check next PC for 0x01 ---

        case 0x80 ... 0x81: // TODO -> i16x8. ~ abs, neg
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0x82: // TODO -> i16x8.q15mulr_sat_s
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0x83 ... 0x84: // TODO -> i16x8. ~ all_true, bitmask
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0x85 ... 0x86: // TODO -> i16x8.narrow_i32x4_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0x87 ... 0x8a: // TODO -> i16x8.extend_(low/high)_i8x16_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0x8b ... 0x8d: // i16x8 shifts
            return next_pc_0x01(m, opcode) && i_instr_simd_shift(m, opcode);

        case 0x8e ... 0x93: // i16x8.(add/sub)
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0x94: // TODO -> f64x2.nearest
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0x95: // i16x8.mul
        case 0x96 ... 0x99: // i16x8.(min/max)
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        // case 0x9a: ! invalid opcode !

        case 0x9b: // TODO -> i16x8.avgr_u
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0x9c ... 0x9f: // TODO -> i16x8.extmul_(low/high)_i8x16_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xa0 ... 0xa1: // TODO -> i32x4. ~ abs, neg
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        // case 0xa2: ! invalid opcode !

        case 0xa3 ... 0xa4: // TODO -> i32x4. ~ all_true, bitmask
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        // case 0xa5 ... 0xa6: ! invalid opcode !

        case 0xa7 ... 0xaa: // TODO -> i32x4.extend_(low/high)_i16x8_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xab ... 0xad: // i32x4 shifts
            return next_pc_0x01(m, opcode) && i_instr_simd_shift(m, opcode);

        case 0xae: // i32x4.add
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        // case 0xaf ... 0xb0: ! invalid opcode !

        case 0xb1: // i32x4.sub
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        // case 0xb2 ... 0xb4: ! invalid opcode !

        case 0xb5: // i32x4.mul
        case 0xb6 ... 0xb9: // i32x4.(min/max)
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0xba: // TODO -> i32x4.dot_i16x8_s
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        // case 0xbb: ! invalid opcode !

        case 0xbc ... 0xbf: // TODO -> i32x4.extmul_(low/high)_i16x8_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xc0 ... 0xc1: // TODO -> i64x2. ~ abs, neg
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        // case 0xc2: ! invalid opcode !

        case 0xc3 ... 0xc4: // TODO -> i64x2. ~ all_true, bitmask
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        // case 0xc5 ... 0xc6: ! invalid opcode !

        case 0xc7 ... 0xca: // TODO -> i64x2.extend_(low/high)_i32x4_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xcb ... 0xcd: // i64x2 shifts
            return next_pc_0x01(m, opcode) && i_instr_simd_shift(m, opcode);

        case 0xce: // i64x2.add
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        // case 0xcf ... 0xd0: ! invalid opcode !

        case 0xd1: // i64x2.sub
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        // case 0xd2 ... 0xd4: ! invalid opcode !

        case 0xd5: // i64x2.mul
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0xd6 ... 0xdb: // i64x2 relational operators
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0xdc ... 0xdf: // TODO -> i64x2.extmul_(low/high)_i32x4_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xe0 ... 0xe1: // TODO -> f32x4. ~ abs, neg
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        // case 0xe2: ! invalid opcode !

        case 0xe3: // TODO -> f32x4.sqrt
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xe4 ... 0xeb: // f32x4. ~ add, sub, mul, div, min, max, pmin, pmax
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0xec ... 0xed: // TODO -> f64x2. ~ abs, neg
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        // case 0xee: ! invalid opcode !
        case 0xef: // TODO -> f64x2.sqrt
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xf0 ... 0xf7: // f64x2. ~ add, sub, mul, div, min, max, pmin, pmax
            return next_pc_0x01(m, opcode) && i_instr_simd_bin_v128_v128_op(m, opcode);

        case 0xf8 ... 0xf9: // TODO -> i32x4.trunc_sat_f32x4_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xfa ... 0xfb: // TODO -> f32x4.convert_i32x4_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xfc ... 0xfd: // TODO -> i32x4.trunc_sat_f64x2_(s/u)_zero
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        case 0xfe ... 0xff: // TODO -> f64x2.convert_low_i32x4_(s/u)
            return next_pc_0x01(m, opcode) && not_implemented(opcode);

        default: {
            sprintf(exception, "unrecognized SIMD opcode 0x%x (0x%x 0x%x)", opcode, 0xfd, opcode);
            return false;
        }
    }
}


void Interpreter::report_overflow([[maybe_unused]] Module *m,
                                  [[maybe_unused]] uint8_t *maddr) {
    dbg_warn("memory start: %p, memory end: %p, maddr: %p\n", m->memory.bytes,
             m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE, maddr);
    sprintf(exception, "out of bounds memory access");
}
