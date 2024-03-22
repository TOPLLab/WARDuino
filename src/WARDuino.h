#pragma once

#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "Debug/debugger.h"
#include "Edward/proxy_supervisor.h"
#include "WARDuino/CallbackHandler.h"

// Constants
#define WA_MAGIC 0x6d736100
#define WA_VERSION 0x01

#define PAGE_SIZE 0x10000      // 65536 bytes TODO
#define STACK_SIZE 0x100       // 65536
#define BLOCKSTACK_SIZE 0x100  // 4096
#define CALLSTACK_SIZE 0x100   // 4096
#define BR_TABLE_SIZE 0x100    // 65536

#define I32 0x7f      // -0x01
#define I64 0x7e      // -0x02
#define F32 0x7d      // -0x03
#define F64 0x7c      // -0x04
#define ANYFUNC 0x70  // -0x10
#define FUNC 0x60     // -0x20
#define BLOCK 0x40    // -0x40

#ifdef ARDUINO
#define EVENTS_SIZE 10
#else
#define EVENTS_SIZE 50
#endif
#define KIND_FUNCTION 0
#define KIND_TABLE 1
#define KIND_MEMORY 2
#define KIND_GLOBAL 3

extern char exception[512];

// Structures
typedef struct Type {
    uint8_t form;
    uint32_t param_count;
    uint32_t *params;
    uint32_t result_count;
    uint32_t *results;
    uint64_t mask;  // unique mask value for each type
} Type;

typedef union FuncPtr {
    void (*void_void)();

    void (*void_i32)(uint32_t);

    void (*void_i64)(uint64_t);

    void (*void_f32)(float);

    void (*void_f64)(double);

    double (*f64_f64)(double);
} FuncPtr;

// A block or function
typedef struct Block {
    uint8_t block_type;         // 0x00: function, 0x01: init_exp, 0x02: block,
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
    std::vector<Block *> functions;   // imported and locally defined functions
    std::map<uint8_t *, Block *>
        block_lookup;  // map of module byte position to Blocks
    // same length as byte_count
    uint32_t start_function = -1;  // function to run on module load
    Table table;
    Memory memory;
    uint32_t global_count = 0;      // number of globals
    std::vector<StackValue> globals;  // globals
    // Runtime state
    uint8_t *pc_ptr = nullptr;     // program counter
    int sp = -1;                   // operand stack pointer
    int fp = -1;                   // current frame pointer into stack
    StackValue *stack;   // main operand stack
    int csp = -1;                  // callstack pointer
    Frame *callstack = nullptr;    // callstack
    uint32_t *br_table = nullptr;  // br_table branch indexes

    char *exception = nullptr;  // exception is set when the program fails
} Module;

typedef bool (*Primitive)(Module *);

typedef struct PrimitiveEntry {
    const char *name;
    Primitive f;
    Type t;
} PrimitiveEntry;

class WARDuino {
   private:
    static WARDuino *singleton;
    std::vector<Module *> modules = {};

    WARDuino();

    uint32_t get_main_fidx(Module *m);

   public:
    Debugger *debugger;
    RunningState program_state = WARDUINOrun;

    static WARDuino *instance();

    int run_module(Module *m);

    Module *load_module(uint8_t *bytes, uint32_t byte_count, Options options);

    void unload_module(Module *m);

    void update_module(Module *old_module, uint8_t *wasm, uint32_t wasm_len);

    bool invoke(Module *m, uint32_t fidx, uint32_t arity = 0,
                StackValue *args = nullptr);

    uint32_t get_export_fidx(Module *m, const char *name);

    void handleInterrupt(size_t len, uint8_t *buff) const;

    void instantiate_module(Module *m, uint8_t *bytes, uint32_t byte_count);

    void free_module_state(Module *m);
};
