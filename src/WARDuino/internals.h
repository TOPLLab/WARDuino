#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#define I32 0x7f  // -0x01
#define I64 0x7e  // -0x02
#define F32 0x7d  // -0x03
#define F64 0x7c  // -0x04

#define I32_8 0x7b   // -0x05
#define I32_16 0x7a  // -0x06
#define I64_8 0x79   // -0x07
#define I64_16 0x78  // -0x08
#define I64_32 0x77  // -0x09

#define I32_8_s 0x7b   // -0x05
#define I32_8_u 0x7a   // -0x06
#define I32_16_s 0x79  // -0x07
#define I32_16_u 0x78  // -0x08
#define I64_8_s 0x77   // -0x09
#define I64_8_u 0x76   // -0x0a
#define I64_16_s 0x75  // -0x0b
#define I64_16_u 0x74  // -0x0c
#define I64_32_s 0x73  // -0x0d
#define I64_32_u 0x72  // -0x0e

#define ANYFUNC 0x70  // -0x10
#define FUNC 0x60     // -0x20
#define BLOCK 0x40    // -0x40

// Structures
typedef struct Type {
    uint8_t form;
    uint32_t param_count;
    uint32_t *params;
    uint32_t result_count;
    uint32_t *results;
    uint64_t mask;  // unique mask value for each type
} Type;

// A block or function
typedef struct Block {
    uint8_t block_type;  // 0x00: function, 0x01: init_exp, 0x02: block,
    // 0x03: loop, 0x04: if, 0xfe: proxy guard,
    // 0xff: cbk guard
    uint32_t fidx;              // function only (index)
    Type *type;                 // params/results type
    uint32_t local_count;       // function only
    uint8_t *local_value_type;  // types of locals (function only)
    uint8_t *start_ptr;
    uint8_t *end_ptr;
    uint8_t *else_ptr;    // if block only
    uint8_t *br_ptr;      // blocks only
    char *export_name;    // function only (exported)
    char *import_module;  // function only (imported)
    char *import_field;   // function only (imported)
    void (*func_ptr)();   // function only (imported)
} Block;

typedef union FuncPtr {
    void (*void_void)();

    void (*void_i32)(uint32_t);

    void (*void_i64)(uint64_t);

    void (*void_f32)(float);

    void (*void_f64)(double);

    double (*f64_f64)(double);
} FuncPtr;

///
typedef struct StackValue {
    uint8_t value_type;
    union {
        uint32_t uint32;
        int32_t int32;
        uint64_t uint64;
        int64_t int64;
        float f32;
        double f64;
    } value;
} StackValue;

typedef struct Frame {
    Block *block;
    // Saved state
    int sp;
    int fp;
    uint8_t *ra_ptr;
} Frame;

///

typedef struct Table {
    uint8_t elem_type = 0;  // type of entries (only ANYFUNC in MVP)
    uint32_t initial = 0;   // initial table size
    uint32_t maximum = 0;   // maximum table size
    uint32_t size = 0;      // current table size
    uint32_t *entries = nullptr;
} Table;

typedef struct Memory {
    uint32_t initial = 0;      // initial size (64K pages)
    uint32_t maximum = 0;      // maximum size (64K pages)
    uint32_t pages = 0;        // current size (64K pages)
    uint8_t *bytes = nullptr;  // memory area
} Memory;

typedef struct Global {
    char *export_name;    // export name of the global
    char *import_module;  // import module name
    char *import_field;   // import field name
    bool mutability;      // 0: immutable, 1: mutable
    StackValue *value;    // current value
} Global;

typedef struct Options {
    // when true: host memory addresses will be outside allocated memory area
    // so do not do bounds checking
    bool disable_memory_bounds;
    // when true, table entries are accessed like this:
    //   m->table.entries[m->table.entries-index]
    // when false, table entires are accessed like this:
    //   m->table.entries[index]
    bool mangle_table_index;
    bool dlsym_trim_underscore;
    bool return_exception;
} Options;

class WARDuino;  // predeclare for it work in the module decl

typedef struct Module {
    WARDuino *warduino = nullptr;
    char *path = nullptr;  // file path of the wasm module
    Options options;       // Config options

    uint32_t byte_count = 0;   // number of bytes in the module
    uint8_t *bytes = nullptr;  // module content/bytes

    uint32_t type_count = 0;  // number of function types
    Type *types = nullptr;    // function types

    uint32_t import_count = 0;    // number of leading imports in functions
    uint32_t function_count = 0;  // number of function (including imports)
    Block *functions = nullptr;   // imported and locally defined functions
    std::unordered_map<uint8_t *, Block *>
        block_lookup;  // map of module byte position to Blocks
    // same length as byte_count
    uint32_t start_function = -1;  // function to run on module load
    Table table;
    Memory memory;
    uint32_t global_count = 0;   // number of globals
    Global **globals = nullptr;  // globals
    // Runtime state
    uint8_t *pc_ptr = nullptr;     // program counter
    int sp = -1;                   // operand stack pointer
    int fp = -1;                   // current frame pointer into stack
    StackValue *stack = nullptr;   // main operand stack
    int csp = -1;                  // callstack pointer
    Frame *callstack = nullptr;    // callstack
    uint32_t *br_table = nullptr;  // br_table branch indexes

    char *exception = nullptr;  // exception is set when the program fails
} Module;

typedef bool (*Primitive)(Module *);

struct IOStateElement {
    std::string key;
    bool output;
    int value;
};

typedef struct PrimitiveEntry {
    const char *name;
    Primitive f;
    void (*f_reverse)(Module *m, std::vector<IOStateElement>);
    void (*f_serialize_state)(std::vector<IOStateElement *> &);
    Type *t;
} PrimitiveEntry;
