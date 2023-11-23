#pragma once
#include "../WARDuino.h"

#ifndef EMULATOR
#define VIRTUAL
#else
#define VIRTUAL virtual
#endif

class Interpreter {
   public:
    /**
     * Push a new frame on to the call stack
     * @param m module
     * @param block
     * @param sp stack pointer to return to
     */
    VIRTUAL void push_block(Module *m, Block *block, int sp);

    /**
     * Restore the top frame from the call stack of a module.
     *
     * Sets stack pointer and call stack pointer, verifies type of return
     *
     * @param m module
     * @return block that was on top of the stack (the one that we just returned to)
     */
    VIRTUAL Block *pop_block(Module *m);

    /**
     * Setup a function
     *
     * Push params and locals on the stack and save a call frame on the call stack
     * Sets new pc value for the start of the function
     *
     * @param m module to work on
     * @param fidx function id (index in m->functions)
     */
    VIRTUAL void setup_call(Module *m, uint32_t fidx);

    /**
     * Start interpretation of the module that has a call set up for it.
     * See #setup_call
     * @param m module that has been set up for a call
     * @param return_exception whether to save the exception method or not
     * @return true if the interpretation ended as expected
     */
    bool interpret(Module *m, bool waiting = false);

   protected:
    VIRTUAL void load(Module *m, uint32_t offset, uint32_t addr, int size, uint8_t value_type, bool sign_extend);
    VIRTUAL void store(Module *m, uint32_t offset, uint32_t addr, int value_sp, int size);

    VIRTUAL bool i_instr_block(Module *m, uint8_t *block_ptr);
    VIRTUAL bool i_instr_loop(Module *m, uint8_t *block_ptr);
    VIRTUAL bool i_instr_if(Module *m, uint8_t *block_ptr);
    VIRTUAL bool i_instr_else(Module *m);
    VIRTUAL bool i_instr_end(Module *m, bool *prog_done);
    VIRTUAL bool i_instr_br(Module *m);
    VIRTUAL bool i_instr_br_if(Module *m);
    VIRTUAL bool i_instr_br_table(Module *m);
    VIRTUAL bool i_instr_return(Module *m);
    VIRTUAL bool i_instr_call(Module *m);
    VIRTUAL bool i_instr_call_indirect(Module *m);
    VIRTUAL bool i_instr_drop(Module *m);
    VIRTUAL bool i_instr_select(Module *m);
    VIRTUAL bool i_instr_get_local(Module *m);
    VIRTUAL bool i_instr_set_local(Module *m);
    VIRTUAL bool i_instr_tee_local(Module *m);
    VIRTUAL bool i_instr_get_global(Module *m);
    VIRTUAL bool i_instr_set_global(Module *m);
    VIRTUAL bool i_instr_current_memory(Module *m);
    VIRTUAL bool i_instr_grow_memory(Module *m);
    VIRTUAL bool i_instr_mem_load(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_mem_store(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_const(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_unary_u32(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_math_u32(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_math_u64(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_math_f32(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_math_f64(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_unary_i32(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_unary_i64(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_unary_floating(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_binary_i32(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_binary_i64(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_binary_f32(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_binary_f64(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_conversion(Module *m, uint8_t opcode);
    VIRTUAL bool i_instr_callback(Module *m, uint8_t opcode);
};