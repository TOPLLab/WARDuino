#include "../WARDuino.h"

// Defining the instruction constants

#define UNREACHABLE 0x00
#define NOP 0x01
#define BLOCK_I 0x02
#define LOOP 0x03
#define IF 0x04
#define ELSE 0x05

#define END 0x0b
#define BR 0x0c
#define BR_IF 0x0d
#define BR_TABLE 0x0e
#define RETURN 0x0f

#define CALL 0x10
#define CALL_INDIRECT 0x11
#define DROP 0x1a
#define SELECT 0x1b
#define GET_LOCAL 0x20
#define SET_LOCAL 0x21
#define TEE_LOCAL 0x22
#define GET_GLOBAL 0x23
#define SET_GLOBAL 0x24
#define CURRENT_MEMORY 0x3f
#define GROW_MEMORY 0x40
#define MEMORY_LOAD_START 0x28
#define MEMORY_LOAD_END 0x35
#define MEMORY_STORE_START 0x36
#define MEMORY_STORE_END 0x3e

#define CONST_START 0x41
#define CONST_END 0x44

#define I32_CONST 0x41
#define I64_CONST 0x42
#define F32_CONST 0x43
#define F64_CONST 0x44

#define F32_ABS 0x8b
#define F32_NEG 0x8c
#define F32_CEIL 0x8d
#define F32_FLOOR 0x8e
#define F32_TRUNC 0x8f
#define F32_NEAREST 0x90
#define F32_SQRT 0x91
#define F64_ABS 0x99
#define F64_NEG 0x9a
#define F64_CEIL 0x9b
#define F64_FLOOR 0x9c
#define F64_TRUNC 0x9d
#define F64_NEAREST 0x9e
#define F64_SQRT 0x9f

// binary boolean operators I32 unsigned int
#define I32_EQ 0x46
#define I32_NE 0x47
#define I32_LT_S 0x48
#define I32_LT_U 0x49
#define I32_GT_S 0x4a
#define I32_GT_U 0x4b
#define I32_LE_S 0x4c
#define I32_LE_U 0x4d
#define I32_GE_S 0x4e
#define I32_GE_U 0x4f

// binary boolean operators I64
#define I64_EQ 0x51
#define I64_NE 0x52
#define I64_LT_S 0x53
#define I64_LT_U 0x54
#define I64_GT_S 0x55
#define I64_GT_U 0x56
#define I64_LE_S 0x57
#define I64_LE_U 0x58
#define I64_GE_S 0x59
#define I64_GE_U 0x5a

#define I32_ADD 0x6a
#define I32_SUB 0x6b
#define I32_MUL 0x6c
#define I32_DIV_S 0x6d
#define I32_DIV_U 0x6e
#define I32_REM_S 0x6f
#define I32_REM_U 0x70
#define I32_AND 0x71
#define I32_OR 0x72
#define I32_XOR 0x73
#define I32_SHL 0x74
#define I32_SHR_S 0x75
#define I32_SHR_U 0x76
#define I32_ROTL 0x77
#define I32_ROTR 0x78

#define F32_ADD 0x92
#define F32_SUB 0x93
#define F32_MUL 0x94
#define F32_DIV 0x95
#define F32_MIN 0x96
#define F32_MAX 0x97
#define F32_COPYSIGN 0x98

#define I64_ADD 0x7c
#define I64_SUB 0x7d
#define I64_MUL 0x7e
#define I64_DIV_S 0x7f
#define I64_DIV_U 0x80
#define I64_REM_S 0x81
#define I64_REM_U 0x82
#define I64_AND 0x83
#define I64_OR 0x84
#define I64_XOR 0x85
#define I64_SHL 0x86
#define I64_SHR_S 0x87
#define I64_SHR_U 0x88
#define I64_ROTL 0x89
#define I64_ROTR 0x8a

#define F64_ADD 0xa0
#define F64_SUB 0xa1
#define F64_MUL 0xa2
#define F64_DIV 0xa3
#define F64_MIN 0xa4
#define F64_MAX 0xa5
#define F64_COPYSIGN 0xa6

#define I32_WRAP_I64 0xa7
#define I32_TRUNC_F32_S 0xa8
#define I32_TRUNC_F32_U 0xa9
#define I32_TRUNC_F64_S 0xaa
#define I32_TRUNC_F64_U 0xab
#define I64_EXTEND_I32_S 0xac
#define I64_EXTEND_I32_U 0xad
#define I64_TRUNC_F32_S 0xae
#define I64_TRUNC_F32_U 0xaf
#define I64_TRUNC_F64_S 0xb0
#define I64_TRUNC_F64_U 0xb1
#define F32_CONVERT_I32_S 0xb2
#define F32_CONVERT_I32_U 0xb3
#define F32_CONVERT_I64_S 0xb4
#define F32_CONVERT_I64_U 0xb5
#define F32_DEMOTE_F64 0xb6
#define F64_CONVERT_I32_S 0xb7
#define F64_CONVERT_I32_U 0xb8
#define F64_CONVERT_I64_S 0xb9
#define F64_CONVERT_I64_U 0xba
#define F64_PROMOTE_F32 0xbb
#define I32_REINTERPRET_F32 0xbc
#define I64_REINTERPRET_F64 0xbd
#define F32_REINTERPRET_I32 0xbe
#define F64_REINTERPRET_I64 0xbf

#define I32_EQ 0x46
#define I32_NE 0x47
#define I32_LT_S 0x48
#define I32_LT_U 0x49
#define I32_GT_S 0x4a
#define I32_GT_U 0x4b
#define I32_LE_S 0x4c
#define I32_LE_U 0x4d
#define I32_GE_S 0x4e
#define I32_GE_U 0x4f

#define I64_EQ 0x51
#define I64_NE 0x52
#define I64_LT_S 0x53
#define I64_LT_U 0x54
#define I64_GT_S 0x55
#define I64_GT_U 0x56
#define I64_LE_S 0x57
#define I64_LE_U 0x58
#define I64_GE_S 0x59
#define I64_GE_U 0x5a

#define F64_EQ 0x61
#define F64_NE 0x62
#define F64_LT 0x63
#define F64_GT 0x64
#define F64_LE 0x65
#define F64_GE 0x66

#define I64_CLZ 0x79
#define I64_CTZ 0x7a
#define I64_POPCNT 0x7b

#define I32_EQZ 0x45
#define I64_EQZ 0x50

#define F32_EQ 0x5b
#define F32_NE 0x5c
#define F32_LT 0x5d
#define F32_GT 0x5e
#define F32_LE 0x5f
#define F32_GE 0x60

#define I32_CLZ 0x67
#define I32_CTZ 0x68
#define I32_POPCNT 0x69

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
