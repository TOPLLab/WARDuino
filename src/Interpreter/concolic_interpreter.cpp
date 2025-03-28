#ifndef ARDUINO
#include "concolic_interpreter.h"

#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "interp_loop.h"

// TODO: Try reuse code from the concrete interpreter
Block *ConcolicInterpreter::pop_block(Module *m) {
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
            m->symbolic_stack[frame->sp + 1] = m->symbolic_stack[m->sp];
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

// TODO: Try reuse code from the concrete interpreter
void ConcolicInterpreter::setup_call(Module *m, uint32_t fidx) {
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
        m->symbolic_stack[m->sp] = encode_as_symbolic(m, &m->stack[m->sp]);
    }

    // Set program counter to start of function
    m->pc_ptr = func->start_ptr;
}

z3::expr ConcolicInterpreter::encode_as_symbolic(Module *m,
                                                 StackValue *stack_value) {
    switch (stack_value->value_type) {
        case I32:
            return m->ctx.bv_val(stack_value->value.int32, 32);
        case I64:
            return m->ctx.bv_val(stack_value->value.int64, 64);
        case F32:
            return m->ctx.fpa_val(stack_value->value.f32);
        case F64:
            return m->ctx.fpa_val(stack_value->value.f64);
        default:
            FATAL("Invalid value_type %d!", stack_value->value_type);
    }
}

void ConcolicInterpreter::load(Module *m, uint32_t offset, uint32_t addr,
                               int size, uint8_t value_type, bool sign_extend) {
    // Load concrete value.
    Interpreter::load(m, offset, addr, size, value_type, sign_extend);

    // Load symbolic value.
    z3::expr_vector expressions(m->ctx);
    for (int i = size - 1; i >= 0; i--) {
        expressions.push_back(
            m->symbolic_memory.symbolic_bytes[offset + addr + i]);
    }
    m->symbolic_stack[m->sp] = z3::concat(expressions).simplify();

    // If the loaded size is smaller than the size of the datatype then we need
    // to sign or zero extend it.
    int full_size = sizeof_valuetype(value_type);
    if (size < full_size) {
        if (sign_extend) {
            m->symbolic_stack[m->sp] = sext(m->symbolic_stack[m->sp].value(),
                                            full_size * 8 - size * 8);
        } else {
            m->symbolic_stack[m->sp] = zext(m->symbolic_stack[m->sp].value(),
                                            full_size * 8 - size * 8);
        }
    }
}

void ConcolicInterpreter::store(Module *m, uint32_t offset, uint32_t addr,
                                int value_sp, int size) {
    // Store concrete value.
    Interpreter::store(m, offset, addr, value_sp, size);

    // Store symbolic value.
    z3::expr symbolic_stack_value = m->symbolic_stack[value_sp].value();

    for (int i = 0; i < size; i++) {
        m->symbolic_memory.symbolic_bytes[offset + addr + i] =
            symbolic_stack_value.extract((i + 1) * 8 - 1, i * 8);
    }
}

z3::expr simplify_bool_conversion(z3::expr expression) {
    if (expression.is_app()) {
        z3::func_decl decl = expression.decl();
        if (decl.decl_kind() == Z3_OP_EQ) {
            if (expression.arg(0).decl().decl_kind() == Z3_OP_ITE) {
                z3::expr iteTrue = expression.arg(0).arg(1);
                z3::expr iteFalse = expression.arg(0).arg(2);
                z3::expr r = expression.arg(1);
                if (iteTrue.is_const() && r.is_const() &&
                    iteTrue.get_numeral_int() == r.get_numeral_int()) {
                    return expression.arg(0).arg(0);
                }
                else if (iteFalse.is_const() && r.is_const() &&
                    iteFalse.get_numeral_int() == r.get_numeral_int()) {
                    return !expression.arg(0).arg(0);
                }
            }
            return expression;
        }
        else if (decl.decl_kind() == Z3_OP_DISTINCT) {
            if (expression.arg(0).decl().decl_kind() == Z3_OP_ITE) {
                z3::expr iteTrue = expression.arg(0).arg(1);
                z3::expr iteFalse = expression.arg(0).arg(2);
                z3::expr r = expression.arg(1);
                if (iteTrue.is_const() && r.is_const() &&
                    iteTrue.get_numeral_int() == r.get_numeral_int()) {
                    return !expression.arg(0).arg(0);
                }
                else if (iteFalse.is_const() && r.is_const() &&
                         iteFalse.get_numeral_int() == r.get_numeral_int()) {
                    return expression.arg(0).arg(0);
                }
            }
            return expression;
        }
        else if (decl.decl_kind() == Z3_OP_ITE) {
        }
        return expression;
    }
    return expression;
}

bool ConcolicInterpreter::i_instr_if(Module *m, uint8_t *block_ptr) {
    uint32_t cond = m->stack[m->sp].value.uint32;
    z3::expr sym_cond = m->symbolic_stack[m->sp].value() != 0;

    sym_cond = simplify_bool_conversion(sym_cond);
    sym_cond = sym_cond.simplify();

    // Update the path condition based on if the branch will be taken in the
    // current execution or not.
    m->path_condition = m->path_condition && (cond ? sym_cond : !sym_cond);

    // Assert that a constant path condition must always be true, if it's false
    // then we are in an impossible branch so something went wrong.
    if (m->path_condition.is_const()) {
        assert(m->path_condition.simplify().is_bool() &&
               m->path_condition.simplify().is_true());
    }

    // Run the concrete implementation of the if instruction.
    return Interpreter::i_instr_if(m, block_ptr);
}

bool ConcolicInterpreter::i_instr_br_if(Module *m) {
    uint32_t cond = m->stack[m->sp].value.uint32;
    z3::expr sym_cond = z3::ite(m->symbolic_stack[m->sp].value() != 0,
                                m->ctx.bool_val(true), m->ctx.bool_val(false));

    // Update the path condition based on if the branch will be taken in the
    // current execution or not.
    m->path_condition = m->path_condition && (cond ? sym_cond : !sym_cond);

    // Run the concrete implementation of the br_if instruction.
    return Interpreter::i_instr_br_if(m);
}

bool ConcolicInterpreter::i_instr_select(Module *m) {
    int original_sp = m->sp;
    Interpreter::i_instr_select(m);
    m->sp = original_sp;

    z3::expr sym_cond = m->symbolic_stack[m->sp--].value();
    m->sp--;
    m->symbolic_stack[m->sp] =
        z3::ite(sym_cond != 0, m->symbolic_stack[m->sp].value(),
                m->symbolic_stack[m->sp + 1].value());

    return true;
}

bool ConcolicInterpreter::i_instr_get_local(Module *m) {
    // Calling Interpreter::i_instr_get_local would just make things a lot more
    // complicated.
    int32_t arg = read_LEB_32(&m->pc_ptr);
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg,
          value_repr(&m->stack[m->fp + arg]));
#endif
    m->stack[++m->sp] = m->stack[m->fp + arg];
    m->symbolic_stack[m->sp] = m->symbolic_stack[m->fp + arg];
    return true;
}

bool ConcolicInterpreter::i_instr_set_local(Module *m) {
    // Calling Interpreter::i_instr_set_local would just make things a lot more
    // complicated.
    int32_t arg = read_LEB_32(&m->pc_ptr);
    m->symbolic_stack[m->fp + arg] = m->symbolic_stack[m->sp];
    m->stack[m->fp + arg] = m->stack[m->sp--];
#if TRACE
    debug("      - arg: 0x%x, to %s (stack loc: %d)\n", arg,
          value_repr(&m->stack[m->sp + 1]), m->fp + arg);
#endif
    return true;
}

bool ConcolicInterpreter::i_instr_tee_local(Module *m) {
    // Calling Interpreter::i_instr_tee_local would just make things a lot more
    // complicated.
    int32_t arg = read_LEB_32(&m->pc_ptr);
    m->stack[m->fp + arg] = m->stack[m->sp];
    // std::cout << "local.tee " << arg << " = " <<
    // m->symbolic_stack[m->sp].value() << std::endl;
    m->symbolic_stack[m->fp + arg] = m->symbolic_stack[m->sp];
#if TRACE
    debug("      - arg: 0x%x, to %s\n", arg, value_repr(&m->stack[m->sp]));
#endif
    return true;
}

bool ConcolicInterpreter::i_instr_get_global(Module *m) {
    int32_t arg = read_LEB_32(&m->pc_ptr);
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg, value_repr(&m->globals[arg]));
#endif
    m->stack[++m->sp] = m->globals[arg];
    m->symbolic_stack[m->sp] = m->symbolic_globals[arg];
    return true;
}

bool ConcolicInterpreter::i_instr_set_global(Module *m) {
    uint32_t arg = read_LEB_32(&m->pc_ptr);
    m->symbolic_globals[arg] = m->symbolic_stack[m->sp].value();
    m->globals[arg] = m->stack[m->sp--];
#if TRACE
    debug("      - arg: 0x%x, got %s\n", arg, value_repr(&m->stack[m->sp + 1]));
#endif
    return true;
}

bool ConcolicInterpreter::i_instr_current_memory(Module *m) {
    Interpreter::i_instr_current_memory(m);
    m->symbolic_stack[m->sp] = m->symbolic_memory.symbolic_pages;
    return true;
}

bool ConcolicInterpreter::i_instr_const(Module *m, uint8_t opcode) {
    Interpreter::i_instr_const(m, opcode);

    // We use the stack value loaded during concrete execution and create a
    // symbolic value from it.
    m->symbolic_stack[m->sp] = encode_as_symbolic(m, &m->stack[m->sp]);

    return true;
}

bool ConcolicInterpreter::i_instr_unary_u32(Module *m, uint8_t opcode) {
    bool success = Interpreter::i_instr_unary_u32(m, opcode);
    m->symbolic_stack[m->sp] = ite(m->symbolic_stack[m->sp].value() == 0,
                                   m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
    return success;
}

bool ConcolicInterpreter::i_instr_math_u32(Module *m, uint8_t opcode) {
    int original_sp = m->sp;
    Interpreter::i_instr_math_u32(m, opcode);
    m->sp = original_sp;

    z3::expr a = m->symbolic_stack[m->sp - 1].value();
    z3::expr b = m->symbolic_stack[m->sp].value();
    std::optional<z3::expr> c;
    m->sp -= 1;
    switch (opcode) {
        case 0x46:
            c = z3::ite(a == b, m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.eq
        case 0x47:
            c = z3::ite(a != b, m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.ne
        case 0x48:
            c = z3::ite(slt(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.lt_s
        case 0x49:
            c = z3::ite(ult(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.lt_u
        case 0x4a:
            c = z3::ite(sgt(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.gt_s
        case 0x4b:
            c = z3::ite(ugt(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.gt_u
        case 0x4c:
            c = z3::ite(sle(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.le_s
        case 0x4d:
            c = z3::ite(ule(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.le_u
        case 0x4e:
            c = z3::ite(sge(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.ge_s
        case 0x4f:
            c = z3::ite(uge(a, b), m->ctx.bv_val(1, 32), m->ctx.bv_val(0, 32));
            break;  // i32.ge_u
        default:
            return false;
    }
    m->symbolic_stack[m->sp] = c;
    return true;
}

bool ConcolicInterpreter::i_instr_math_u64(Module *m, uint8_t opcode) {
    int original_sp = m->sp;
    Interpreter::i_instr_math_u64(m, opcode);
    m->sp = original_sp;

    z3::expr d = m->symbolic_stack[m->sp - 1].value();
    z3::expr e = m->symbolic_stack[m->sp].value();
    std::optional<z3::expr> c;
    m->sp -= 1;
    switch (opcode) {
        case 0x51:
            c = z3::ite(d == e, m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.eq
        case 0x52:
            c = z3::ite(d != e, m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.ne
        case 0x53:
            c = z3::ite(slt(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.lt_s
        case 0x54:
            c = z3::ite(ult(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.lt_u
        case 0x55:
            c = z3::ite(sgt(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.gt_s
        case 0x56:
            c = z3::ite(ugt(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.gt_u
        case 0x57:
            c = z3::ite(sle(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.le_s
        case 0x58:
            c = z3::ite(ule(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.le_u
        case 0x59:
            c = z3::ite(sge(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.ge_s
        case 0x5a:
            c = z3::ite(uge(d, e), m->ctx.bv_val(1, 64), m->ctx.bv_val(0, 64));
            break;  // i64.ge_u
        default:
            return false;
    }
    m->symbolic_stack[m->sp] = c;
    return true;
}

bool ConcolicInterpreter::i_instr_math_f32(Module *m, uint8_t opcode) {
    // TODO: Symbolic semantics
    assert(false);
}

bool ConcolicInterpreter::i_instr_math_f64(Module *m, uint8_t opcode) {
    // TODO: Symbolic semantics
    assert(false);
}

bool ConcolicInterpreter::i_instr_unary_i32(Module *m, uint8_t opcode) {
    z3::expr e = m->symbolic_stack[m->sp].value().simplify();
    switch (opcode) {
        case 0x67: { // i32.clz
            z3::expr result =  m->ctx.bv_val(32, 32);
            for (int i = 0; i < 32; i++) {
                result = z3::ite((e & m->ctx.bv_val(1 << i, 32)) >= 1, m->ctx.bv_val(32-i-1, 32), result);
            }
            m->symbolic_stack[m->sp] = result;
            break;
        }
        case 0x68: { // i32.ctz
            z3::expr result =  m->ctx.bv_val(32, 32);
            for (int i = 31; i >= 0; i--) {
                result = z3::ite((e & m->ctx.bv_val(1 << i, 32)) >= 1, m->ctx.bv_val(i, 32), result);
            }
            m->symbolic_stack[m->sp] = result;
            break;
        }
        case 0x69: // i32.popcnt
            assert(false); // TODO: Symbolic semantics
        default:
            return false;
    }
    Interpreter::i_instr_unary_i32(m, opcode);
    return true;
}

bool ConcolicInterpreter::i_instr_unary_i64(Module *m, uint8_t opcode) {
    // TODO: Symbolic semantics
    assert(false);
}

bool ConcolicInterpreter::i_instr_unary_floating(Module *m, uint8_t opcode) {
    switch (opcode) {
        // unary f32
        case 0x8b:
            m->stack[m->sp].value.f32 = fabs(m->stack[m->sp].value.f32);
            m->symbolic_stack[m->sp] =
                z3::abs(m->symbolic_stack[m->sp].value());
            break;  // f32.abs
        case 0x8c:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.neg
        case 0x8d:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.ceil
        case 0x8e:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.floor
        case 0x8f:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.trunc
        case 0x90:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.nearest
        case 0x91:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.sqrt

            // unary f64
        case 0x99:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.abs
        case 0x9a:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.neg
        case 0x9b:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.ceil
        case 0x9c:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.floor
        case 0x9d:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.trunc
        case 0x9e:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.nearest
        case 0x9f:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.sqrt
        default:
            return false;
    }
    return true;
}

bool ConcolicInterpreter::i_instr_binary_i32(Module *m, uint8_t opcode) {
    int original_sp = m->sp;
    Interpreter::i_instr_binary_i32(m, opcode);
    m->sp = original_sp;

    z3::expr a = m->symbolic_stack[m->sp - 1].value();
    z3::expr b = m->symbolic_stack[m->sp].value();
    std::optional<z3::expr> c;
    m->sp -= 1;
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
            c = a / b;
            break;  // i32.div_s
        case 0x6e:
            c = z3::udiv(a, b);
            break;  // i32.div_u
        case 0x6f:
            c = z3::srem(a, b);
            break;  // i32.rem_s
        case 0x70:
            c = z3::urem(a, b);
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
            c = shl(a, b);
            break;  // i32.shl
        case 0x75:
            c = ashr(a, b);
            break;  // i32.shr_s
        case 0x76:
            c = lshr(a, b);
            break;  // i32.shr_u
        case 0x77:
            c = z3::shl(a, b) | z3::lshr(a, 32 - b);
            break;  // i32.rotl
        case 0x78:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i32.rotr
        default:
            return false;
    }
    m->symbolic_stack[m->sp] = c;
    return true;
}

bool ConcolicInterpreter::i_instr_binary_i64(Module *m, uint8_t opcode) {
    int original_sp = m->sp;
    Interpreter::i_instr_binary_i64(m, opcode);
    m->sp = original_sp;

    z3::expr d = m->symbolic_stack[m->sp - 1].value();
    z3::expr e = m->symbolic_stack[m->sp].value();
    std::optional<z3::expr> f;
    m->sp -= 1;
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
            f = d / e;
            break;  // i64.div_s
        case 0x80:
            f = z3::udiv(d, e);
            break;  // i64.div_u
        case 0x81:
            f = z3::srem(d, e);
            break;  // i64.rem_s
        case 0x82:
            f = z3::urem(d, e);
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
            f = z3::shl(d, e);
            break;  // i64.shl
        case 0x87:
            f = z3::ashr(d, e);
            break;  // i64.shr_s
        case 0x88:
            f = z3::lshr(d, e);
            break;  // i64.shr_u
        case 0x89:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i64.rotl
        case 0x8a:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i64.rotr
        default:
            return false;
    }
    m->symbolic_stack[m->sp] = f;

    return true;
}

bool ConcolicInterpreter::i_instr_binary_f32(Module *m, uint8_t opcode) {
    // TODO: Symbolic semantics
    assert(false);
    return false;
}

bool ConcolicInterpreter::i_instr_binary_f64(Module *m, uint8_t opcode) {
    // TODO: Symbolic semantics
    assert(false);
    return false;
}

bool ConcolicInterpreter::i_instr_conversion(Module *m, uint8_t opcode) {
    int original_sp = m->sp;
    Interpreter::i_instr_conversion(m, opcode);
    m->sp = original_sp;

    switch (opcode) {
        case 0xa7:
            m->symbolic_stack[m->sp] = m->symbolic_stack[m->sp]->extract(31, 0);
            break;  // i32.wrap/i64
        case 0xa8:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i32.trunc_s/f32
        case 0xa9:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i32.trunc_u/f32
        case 0xaa:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i32.trunc_s/f64
        case 0xab:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i32.trunc_u/f64
        case 0xac:
            m->symbolic_stack[m->sp] =
                sext(m->symbolic_stack[m->sp].value(), 32);
            break;  // i64.extend_s/i32
        case 0xad:
            m->symbolic_stack[m->sp] =
                zext(m->symbolic_stack[m->sp].value(), 32);
            break;  // i64.extend_u/i32
        case 0xae:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i64.trunc_s/f32
        case 0xaf:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i64.trunc_u/f32
        case 0xb0:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i64.trunc_s/f64
        case 0xb1:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i64.trunc_u/f64
        case 0xb2:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.convert_s/i32
        case 0xb3:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.convert_u/i32
        case 0xb4:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.convert_s/i64
        case 0xb5:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.convert_u/i64
        case 0xb6:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.demote/f64
        case 0xb7:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.convert_s/i32
        case 0xb8:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.convert_u/i32
        case 0xb9:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.convert_s/i64
        case 0xba:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.convert_u/i64
        case 0xbb:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.promote/f32

            // reinterpretations
        case 0xbc:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i32.reinterpret/f32
        case 0xbd:
            // TODO: Symbolic semantics
            assert(false);
            break;  // i64.reinterpret/f64
        case 0xbe:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f32.reinterpret/i32
        case 0xbf:
            // TODO: Symbolic semantics
            assert(false);
            break;  // f64.reinterpret/i64
        default:
            return false;
    }

    return true;
}

bool ConcolicInterpreter::interpret(Module *m, bool waiting) {
    return interp<ConcolicInterpreter>(m , waiting);
}
#endif
