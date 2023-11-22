#pragma once
#include "interpreter.h"

class ConcolicInterpreter : public Interpreter {
   public:
    Block *pop_block(Module *m) override;
    void setup_call(Module *m, uint32_t fidx) override;

    static z3::expr encode_as_symbolic(Module *m, StackValue *stack_value);

   private:
    void load(Module *m, uint32_t offset, uint32_t addr, int size, uint8_t value_type, bool sign_extend) override;
    void store(Module *m, uint32_t offset, uint32_t addr, int value_sp, int size) override;

    bool i_instr_if(Module *m, uint8_t *block_ptr) override;
    bool i_instr_br_if(Module *m) override;
    bool i_instr_select(Module *m) override;
    bool i_instr_get_local(Module *m) override;
    bool i_instr_set_local(Module *m) override;
    bool i_instr_tee_local(Module *m) override;
    bool i_instr_get_global(Module *m) override;
    bool i_instr_set_global(Module *m) override;
    bool i_instr_current_memory(Module *m) override;
    bool i_instr_const(Module *m, uint8_t opcode) override;
    bool i_instr_unary_u32(Module *m, uint8_t opcode) override;
    bool i_instr_math_u32(Module *m, uint8_t opcode) override;
    bool i_instr_math_u64(Module *m, uint8_t opcode) override;
    bool i_instr_math_f32(Module *m, uint8_t opcode) override;
    bool i_instr_math_f64(Module *m, uint8_t opcode) override;
    bool i_instr_unary_i32(Module *m, uint8_t opcode) override;
    bool i_instr_unary_i64(Module *m, uint8_t opcode) override;
    bool i_instr_unary_floating(Module *m, uint8_t opcode) override;
    bool i_instr_binary_i32(Module *m, uint8_t opcode) override;
    bool i_instr_binary_i64(Module *m, uint8_t opcode) override;
    bool i_instr_binary_f32(Module *m, uint8_t opcode) override;
    bool i_instr_binary_f64(Module *m, uint8_t opcode) override;
    bool i_instr_conversion(Module *m, uint8_t opcode) override;
};
