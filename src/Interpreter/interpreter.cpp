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


void Interpreter::threadedInterpreter(Module *m)
{
    // instruction states and local variables
    
    uint8_t opcode;
    uint8_t* block_ptr;
    StackValue *target;

    int32_t arg0;
    int32_t arg1;
    int32_t result;

    Block *block = nullptr;
    uint32_t fidx;
    std::__1::map<uint8_t *, Block *>::iterator block_itr;

    // Jump table for instruction dispatch
    void* jumpTable[256] = {0};
    
    //set all instructions to undefined
    for(int i = 0; i < 256; i++) {
        jumpTable[i] = &&undefinedInstruction;
    }

    // set the jump table for the instructions
    jumpTable[I32_CONST] = &&i32_const;  
    jumpTable[SET_LOCAL] = &&set_local;
    jumpTable[GET_LOCAL] = &&get_local;
    jumpTable[LOOP] = &&loop;
    jumpTable[BLOCK] = &&block;
    jumpTable[END] = &&end;
    jumpTable[CALL] = &&call;
    jumpTable[BR_IF] = &&br_if;
    jumpTable[BR] = &&br;
    jumpTable[RETURN] = &&ret;
    jumpTable[TEE_LOCAL] = &&tee_local;
    jumpTable[SELECT] = &&i_instr_select;

    // boolean instructions for 32 bit 
    jumpTable[I32_EQ]   = &&math_binop_32_bool;
    jumpTable[I32_NE]   = &&math_binop_32_bool;
    jumpTable[I32_LT_S] = &&math_binop_32_bool;
    jumpTable[I32_LT_U] = &&math_binop_32_bool;
    jumpTable[I32_GT_S] = &&math_binop_32_bool;
    jumpTable[I32_GT_U] = &&math_binop_32_bool;
    jumpTable[I32_LE_S] = &&math_binop_32_bool;
    jumpTable[I32_LE_U] = &&math_binop_32_bool;
    jumpTable[I32_GE_S] = &&math_binop_32_bool;
    jumpTable[I32_GE_U] = &&math_binop_32_bool;
    // boolean instructions for 64 bit
    jumpTable[I64_EQ]   = &&math_binop_64_bool;
    jumpTable[I64_NE]   = &&math_binop_64_bool;
    jumpTable[I64_LT_S] = &&math_binop_64_bool;
    jumpTable[I64_LT_U] = &&math_binop_64_bool;
    jumpTable[I64_GT_S] = &&math_binop_64_bool;
    jumpTable[I64_GT_U] = &&math_binop_64_bool;
    jumpTable[I64_LE_S] = &&math_binop_64_bool;
    jumpTable[I64_LE_U] = &&math_binop_64_bool;
    jumpTable[I64_GE_S] = &&math_binop_64_bool;
    jumpTable[I64_GE_U] = &&math_binop_64_bool;
    //binop instructions for 32 bit 
    jumpTable[I32_ADD] = &&math_binop_32_int;
    jumpTable[I32_SUB] = &&math_binop_32_int;
    jumpTable[I32_MUL] = &&math_binop_32_int;
    jumpTable[I32_DIV_S] = &&math_binop_32_int;
    jumpTable[I32_DIV_U] = &&math_binop_32_int;
    jumpTable[I32_REM_S] = &&math_binop_32_int;
    jumpTable[I32_REM_U] = &&math_binop_32_int;
    jumpTable[I32_AND] = &&math_binop_32_int;
    jumpTable[I32_OR] = &&math_binop_32_int;
    jumpTable[I32_XOR] = &&math_binop_32_int;
    jumpTable[I32_SHL] = &&math_binop_32_int;
    jumpTable[I32_SHR_S] = &&math_binop_32_int;
    jumpTable[I32_SHR_U] = &&math_binop_32_int;
    jumpTable[I32_ROTL] = &&math_binop_32_int;
    jumpTable[I32_ROTR] = &&math_binop_32_int;
    
    jumpTable[I32_EQZ] = &&i32_eqz;
    jumpTable[I64_EQZ] = &&i64_eqz;

    goto nextInstruction;

    // Basis set of instructions for implementing fibonacci
    /*
        I32 CONST 
     */ 
    i32_const:
        target = &m->stack[++m->sp];
        target->value_type = I32;
        target->value.uint32 = read_LEB_signed(&m->pc_ptr, 32);
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

    end: 
        block = m->warduino->interpreter->pop_block(m);
        
        if (block == nullptr) {
            return;  // an exception (set by pop_block)
        }

        if (block->block_type == 0x00) {  // Function
            if (m->csp == -1) {
                return ;  // break loop
            } else {
                // Keep going at return address
            }
        } else if (block->block_type == 0x01) {  // init_expr
            return;  // // break loop
        } else {       
            
        }
        goto nextInstruction;  

    br_if: 
        // condition
        arg0 = read_LEB_32(&m->pc_ptr);
        arg1 = m->stack[m->sp--].value.uint32;
        if (arg1) {  // if true
            m->csp -= arg0;
            // set to end for pop_block
            m->pc_ptr = m->callstack[m->csp].block->br_ptr;
        }   
        goto nextInstruction; 

    br:
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

    

    //Binary boolean operators 32 bit
    math_binop_32_bool:
        arg0 = m->stack[m->sp - 1].value.uint32;
        arg1 = m->stack[m->sp].value.uint32;
        m->sp -= 1;
        switch (opcode) {
            case I32_EQ:
                result = static_cast<uint32_t>(arg0 == arg1);
                break;  
            case I32_NE:
                result = static_cast<uint32_t>(arg0 != arg1);
                break; 
            case I32_LT_S:
                result = static_cast<uint32_t>((int32_t)arg0 < (int32_t)arg1);
                break;  
            case I32_LT_U:
                result = static_cast<uint32_t>(arg0 < arg1);
                break; 
            case I32_GT_S:
                result = static_cast<uint32_t>((int32_t)arg0 > (int32_t)arg1);
                break;  
            case I32_GT_U:
                result = static_cast<uint32_t>(arg0 > arg1);
                break;  
            case I32_LE_S:
                result = static_cast<uint32_t>((int32_t)arg0 <= (int32_t)arg1);
                break; 
            case I32_LE_U:
                result = static_cast<uint32_t>(arg0 <= arg1);
                break;  
            case I32_GE_S:
                result = static_cast<uint32_t>((int32_t)arg0 >= (int32_t)arg1);
                break;  
            case I32_GE_U:
                result = static_cast<uint32_t>(arg0 >= arg1);
                break; 
            default:
                printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
                exit(1);
        }
        // m->stack[m->sp].value_type = I32; (This is already set to I32 in the stack)
        m->stack[m->sp].value.uint32 = result;
        goto nextInstruction;

    math_binop_32_int: 
        // TODO: verify if this should not be done with int32_t instead
        arg0 = m->stack[m->sp - 1].value.uint32;
        arg1 = m->stack[m->sp].value.uint32;
        m->sp -= 1;

        if (opcode >= 0x6d && opcode <= 0x70 && arg1 == 0) {
            sprintf(exception, "integer divide by zero");
            exit(1);
        }
        switch (opcode) {
            // case 0x6a: o = __builtin_add_overflow(a, b, &c); break;
            // // i32.add case 0x6b: o = __builtin_sub_overflow(a, b,
            // &c); break;  // i32.sub
            case I32_ADD:
                result = arg0 + arg1;
                break;  // i32.add
            case I32_SUB:
                result = arg0 - arg1;
                break;  // i32.sub
            case I32_MUL:
                result = arg0 * arg1;
                break;  // i32.mul
            case I32_DIV_S:
                if (arg0 == 0x80000000 && arg1 == (uint32_t)-1) {
                    sprintf(exception, "integer overflow");
                    exit(1);
                }
                result = (int32_t)arg0 / (int32_t)arg1;
                break;  // i32.div_s
            case I32_DIV_U:
                result = arg0 / arg1;
                break;  // i32.div_u
            case I32_REM_S:
                if (arg0 == 0x80000000 && arg1 == (uint32_t)-1) {
                    result = 0;
                } else {
                    result = (int32_t)arg0 % (int32_t)arg1;
                };
                break;  // i32.rem_s
            case I32_REM_U:
                result = arg0 % arg1;
                break;  // i32.rem_u
            case I32_AND:
                result = arg0 & arg1;
                break;  // i32.and
            case I32_OR:
                result = arg0 | arg1;
                break;  // i32.or
            case I32_XOR:
                result = arg0 ^ arg1;
                break;  // i32.xor
            case I32_SHL:
                result = arg0 << arg1;
                break;  // i32.shl
            case I32_SHR_S:
                result = (int32_t)arg0 >> arg1;  // NOLINT(hicpp-signed-bitwise)
                break;                // i32.shr_s
            case I32_SHR_U:
                result = arg0 >> arg1;
                break;  // i32.shr_u
            case I32_ROTL:
                result = rotl32(arg0, arg1);
                break;  // i32.rotl
            case I32_ROTR:
                result = rotr32(arg0, arg1);
                break;  // i32.rotr
            default:
                exit(1);
        }
        // m->stack[m->sp].value_type = I32; (This is already set to I32 in the stack)
        m->stack[m->sp].value.uint32 = result;
        goto nextInstruction;

    math_binop_64_bool:
        arg0 = m->stack[m->sp - 1].value.uint64;
        arg1 = m->stack[m->sp].value.uint64;
        m->sp -= 1;
        switch (opcode) {
            case I64_EQ:
                result = static_cast<uint32_t>(arg0 == arg1);
                break;  
            case I64_NE:
                result = static_cast<uint32_t>(arg0 != arg1);
                break;  
            case I64_LT_S:
                result = static_cast<uint32_t>((int64_t)arg0< (int64_t)arg1);
                break;  
            case I64_LT_U:
                result = static_cast<uint32_t>(arg0 < arg1);
                break;  
            case I64_GT_S:
                result = static_cast<uint32_t>((int64_t)arg0 > (int64_t)arg1);
                break;  
            case I64_GT_U:
                result = static_cast<uint32_t>(arg0 > arg1);
                break;  
            case I64_LE_S:
                result = static_cast<uint32_t>((int64_t)arg0 <= (int64_t)arg1);
                break; 
            case I64_LE_U:
                result = static_cast<uint32_t>(arg0 <= arg1);
                break;  
            case I64_GE_S:
                result = static_cast<uint32_t>((int64_t)arg0 >= (int64_t)arg1);
                break;  
            case I64_GE_U:
                result = static_cast<uint32_t>(arg0 >= arg1);
                break;  
            default:
                printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
                exit(1);
        }
        m->stack[m->sp].value_type = I32;
        m->stack[m->sp].value.uint32 = result;   
        goto nextInstruction;

    i32_eqz:  // i32.eqz
        m->stack[m->sp].value.uint32 = static_cast<uint32_t>(m->stack[m->sp].value.uint32 == 0);
        goto nextInstruction;

    i64_eqz:  // i64.eqz
        m->stack[m->sp].value_type = I32;
        m->stack[m->sp].value.uint32 = static_cast<uint32_t>(m->stack[m->sp].value.uint64 == 0);
        goto nextInstruction;


    // Administrative instructions 

    nextInstruction:
        //dbg_dump_stack(m);
        opcode = *m->pc_ptr;
        block_ptr = m->pc_ptr;
        m->pc_ptr += 1;
        goto *jumpTable[opcode];

    undefinedInstruction:
        printf("ran into an undefined instruction:: 0x%02hhX ", opcode);
        exit(0);
    
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
            case BLOCK:  
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

void Interpreter::report_overflow(Module *m, uint8_t *maddr) {
    dbg_warn("memory start: %p, memory end: %p, maddr: %p\n", m->memory.bytes,
             m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE, maddr);
    sprintf(exception, "out of bounds memory access");
}
