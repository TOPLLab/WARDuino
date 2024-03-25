#include "../WARDuino.h"

bool i_instr_block(Module *m, uint8_t *block_ptr);

bool i_instr_loop(Module *m, uint8_t *block_ptr);

bool i_instr_if(Module *m, uint8_t *block_ptr);

bool i_instr_else(Module *m);

bool i_instr_end(Module *m, bool *prog_done);

bool i_instr_br(Module *m);

bool i_instr_br_if(Module *m);

bool i_instr_br_table(Module *m);

bool i_instr_return(Module *m);

bool i_instr_call(Module *m);

bool i_instr_call_indirect(Module *m);

bool i_instr_drop(Module *m);

bool i_instr_select(Module *m);

bool i_instr_get_local(Module *m);

bool i_instr_set_local(Module *m);

bool i_instr_tee_local(Module *m);

bool i_instr_get_global(Module *m);

bool i_instr_set_global(Module *m);

bool i_instr_current_memory(Module *m);

bool i_instr_grow_memory(Module *m);

bool i_instr_mem_load(Module *m, uint8_t opcode);

bool i_instr_mem_store(Module *m, uint8_t opcode);

bool i_instr_const(Module *m, uint8_t opcode);

bool i_instr_unary_u32(Module *m, uint8_t opcode);

bool i_instr_math_u32(Module *m, uint8_t opcode);

bool i_instr_math_u64(Module *m, uint8_t opcode);

bool i_instr_math_f32(Module *m, uint8_t opcode);

bool i_instr_math_f64(Module *m, uint8_t opcode);

bool i_instr_unary_i32(Module *m, uint8_t opcode);

bool i_instr_unary_i64(Module *m, uint8_t opcode);

bool i_instr_unary_floating(Module *m, uint8_t opcode);

bool i_instr_binary_i32(Module *m, uint8_t opcode);

bool i_instr_binary_i64(Module *m, uint8_t opcode);

bool i_instr_binary_f32(Module *m, uint8_t opcode);

bool i_instr_binary_f64(Module *m, uint8_t opcode);

bool i_instr_conversion(Module *m, uint8_t opcode);

bool i_instr_callback(Module *m, uint8_t opcode);
