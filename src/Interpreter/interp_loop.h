#pragma once

#include <iostream>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"

/*template bool interp<ConcolicInterpreter>(Module *m, bool waiting);
template bool interp<Interpreter>(Module *m, bool waiting);*/

template <typename T>
bool interp(Module *m, bool waiting) {
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

        m->path_condition = m->path_condition.simplify();
        /*std::string symbolic_stack_str = "sym stack    : [ ";
        std::string stack_str = "stack        : [ ";
        //for (int i = std::max(0, m->sp - 5); i <= m->sp; i++) {
        for (int i = 0; i <= m->sp; i++) {
            stack_str += std::to_string(m->stack[i].value.int32);
            if (m->symbolic_stack[i].has_value()) {
                auto v = m->symbolic_stack[i].value().simplify();
                assert(v.to_string() != "null");
                if (v.is_bv() && v.is_numeral()) {
                    symbolic_stack_str +=
        std::to_string(v.get_numeral_uint64()); } else { symbolic_stack_str +=
        v.to_string();
                }
            }
            else {
                // None means that there is no symbolic value for the current
        //thing on the stack, we didn't implement something yet in that case.
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
        std::cout << stack_str << std::endl;

        z3::solver s(m->ctx);
        s.add(m->path_condition);
        auto result = s.check();
        std::cout << "Path condition: " << m->path_condition.to_string() << std::endl;
        assert(result == z3::sat);

        std::cout << "Globals: [";
        for (int i = 0; i < m->symbolic_globals.size(); i++) {
            std::cout << m->symbolic_globals[i].to_string() << ", ";
        }
        std::cout << std::endl;*/

        /*std::cout << "running instr: " << OPERATOR_INFO[opcode]; // <<
        std::endl; uint8_t *pc_ptr_tmp = m->pc_ptr; std::cout << " " <<
        read_LEB_32(&pc_ptr_tmp) << std::endl;*/
        if (m->warduino->program_state == WARDUINOrun) {
            m->instructions_executed++;
            if (m->warduino->max_instructions > 0 &&
                m->instructions_executed >= m->warduino->max_instructions) {
                debug("Max instructions executed\n");
                return true;
            }

            if (m->warduino->stop_at_pc >= 0) {
                if (m->pc_ptr == toPhysicalAddress(m->warduino->stop_at_pc, m)) {
                    debug("Stop pc reached\n");
                    return true;
                }
            }
        }
        //if (m->warduino->max_symbolic_variables > 0 && m->symbolic_variable_count > m->warduino->max_symbolic_variables) {
        if (m->warduino->stop) {
            return true;
        }
        T interpreter;
        /*if (std::is_same<T, Interpreter>()) {
            printf("Concrete\n");
        } else if (std::is_same<T, ConcolicInterpreter>()) {
            printf("Concolic\n");
        }*/
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
                success &= interpreter.i_instr_block(m, block_ptr);
                continue;
            case 0x03:  // loop
                success &= interpreter.i_instr_loop(m, block_ptr);
                continue;
            case 0x04:  // if
                success &= interpreter.i_instr_if(m, block_ptr);
                continue;
            case 0x05:  // else
                success &= interpreter.i_instr_else(m);
                continue;
            case 0x0b:  // end
                success &= interpreter.i_instr_end(m, &program_done);
                continue;
            case 0x0c:  // br
                success &= interpreter.i_instr_br(m);
                continue;
            case 0x0d:  // br_if
                success &= interpreter.i_instr_br_if(m);
                continue;
            case 0x0e:  // br_table
                success &= interpreter.i_instr_br_table(m);
                continue;
            case 0x0f:  // return
                success &= interpreter.i_instr_return(m);
                continue;

                //
                // Call operators
                //
            case 0x10: {  // call
                success &= interpreter.i_instr_call(m);
                continue;
            }
            case 0x11: {  // call_indirect
                success &= interpreter.i_instr_call_indirect(m);
                continue;
            }
                //
                // Parametric operators
                //
            case 0x1a:  // drop
                success &= interpreter.i_instr_drop(m);
                continue;
            case 0x1b:  // select
                success &= interpreter.i_instr_select(m);
                continue;

                //
                // Variable access
                //
            case 0x20:  // get_local
                success &= interpreter.i_instr_get_local(m);
                continue;
            case 0x21:  // set_local
                success &= interpreter.i_instr_set_local(m);
                continue;
            case 0x22:  // tee_local
                success &= interpreter.i_instr_tee_local(m);
                continue;
            case 0x23:  // get_global
                success &= interpreter.i_instr_get_global(m);
                continue;
            case 0x24:  // set_global
                success &= interpreter.i_instr_set_global(m);
                continue;

                //
                // Memory-related operators
                //
            case 0x3f:  // current_memory
                success &= interpreter.i_instr_current_memory(m);
                continue;
            case 0x40:  // grow_memory
                success &= interpreter.i_instr_grow_memory(m);
                continue;
                // Memory load operators
            case 0x28 ... 0x35:
                success &= interpreter.i_instr_mem_load(m, opcode);
                continue;
                // Memory store operators
            case 0x36 ... 0x3e:
                success &= interpreter.i_instr_mem_store(m, opcode);
                continue;

                //
                // Constants
                //
            case 0x41 ... 0x44:  // i32.const
                success &= interpreter.i_instr_const(m, opcode);
                continue;

                //
                // Comparison operators
                //

                // unary
            case 0x45:  // i32.eqz
            case 0x50:  // i64.eqz
                success &= interpreter.i_instr_unary_u32(m, opcode);
                continue;

                // i32 binary
            case 0x46 ... 0x4f:
                success &= interpreter.i_instr_math_u32(m, opcode);
                continue;
            case 0x51 ... 0x5a:
                success &= interpreter.i_instr_math_u64(m, opcode);
                continue;
            case 0x5b ... 0x60:
                success &= interpreter.i_instr_math_f32(m, opcode);
                continue;
            case 0x61 ... 0x66:
                success &= interpreter.i_instr_math_f64(m, opcode);
                continue;

                //
                // Numeric operators
                //

                // unary i32
            case 0x67 ... 0x69:
                success &= interpreter.i_instr_unary_i32(m, opcode);
                continue;

                // unary i64
            case 0x79 ... 0x7b:
                success &= interpreter.i_instr_unary_i64(m, opcode);
                continue;

            case 0x8b ... 0x91:  // unary f32
            case 0x99 ... 0x9f:  // unary f64
                success &= interpreter.i_instr_unary_floating(m, opcode);
                continue;

                // i32 binary
            case 0x6a ... 0x78:
                success &= interpreter.i_instr_binary_i32(m, opcode);
                continue;

                // i64 binary
            case 0x7c ... 0x8a:
                success &= interpreter.i_instr_binary_i64(m, opcode);
                continue;

                // f32 binary
            case 0x92 ... 0x98:
                success &= interpreter.i_instr_binary_f32(m, opcode);
                continue;

                // f64 binary
            case 0xa0 ... 0xa6:
                success &= interpreter.i_instr_binary_f64(m, opcode);
                continue;

                // conversion operations
            case 0xa7 ... 0xbb:
                success &= interpreter.i_instr_conversion(m, opcode);
                continue;

                // callback operations
            case 0xe0 ... 0xe3:
                success &= interpreter.i_instr_callback(m, opcode);
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
