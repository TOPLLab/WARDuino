#ifndef WAC_H
#define WAC_H

#include <array>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <map>
#include <queue>  // std::queue
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

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

#define EVENTS_SIZE 1

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
    uint8_t block_type;  // 0x00: function, 0x01: init_exp
    // 0x02: block, 0x03: loop, 0x04: if
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
    uint8_t elem_type;  // type of entries (only ANYFUNC in MVP)
    uint32_t initial;   // initial table size
    uint32_t maximum;   // maximum table size
    uint32_t size;      // current table size
    uint32_t *entries;
} Table;

typedef struct Memory {
    uint32_t initial;  // initial size (64K pages)
    uint32_t maximum;  // maximum size (64K pages)
    uint32_t pages;    // current size (64K pages)
    uint8_t *bytes;    // memory area
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
    WARDuino *warduino;
    char *path;       // file path of the wasm module
    Options options;  // Config options

    uint32_t byte_count;  // number of bytes in the module
    uint8_t *bytes;       // module content/bytes

    uint32_t type_count;  // number of function types
    Type *types;          // function types

    uint32_t import_count;    // number of leading imports in functions
    uint32_t function_count;  // number of function (including imports)
    Block *functions;         // imported and locally defined functions
    std::map<uint8_t *, Block *>
        block_lookup;  // map of module byte position to Blocks
    // same length as byte_count
    uint32_t start_function;  // function to run on module load
    Table table;
    Memory memory;
    uint32_t global_count;  // number of globals
    StackValue *globals;    // globals
    // Runtime state
    uint8_t *pc_ptr;     // program counter
    int sp;              // operand stack pointer
    int fp;              // current frame pointer into stack
    StackValue *stack;   // main operand stack
    int csp;             // callstack pointer
    Frame *callstack;    // callstack
    uint32_t *br_table;  // br_table branch indexes

    char *exception;  // exception is set when the program fails
} Module;

typedef bool (*Primitive)(Module *);

typedef struct PrimitiveEntry {
    const char *name;
    Primitive f;
    Type t;
} PrimitiveEntry;

enum RunningState { WARDUINOrun, WARDUINOpause, WARDUINOstep };

class Event {
   public:
    std::string topic;
    const char *payload;

    Event(std::string topic, const char *payload);
};

class Callback {
   private:
    Module *module;  // reference to module
   public:
    std::string topic;
    uint32_t table_index{};

    explicit Callback(Module *m, std::string id, uint32_t tidx);
    Callback(const Callback &c);

    void resolve_event(const Event &e);
};

class CallbackHandler {
   private:
    static std::unordered_map<std::string, std::vector<Callback> *> *callbacks;
    static std::queue<Event> *events;

    CallbackHandler() = default;  // Disallow creation

   public:
    //    static bool resolving_event;

    static void add_callback(const Callback &c);
    static void remove_callback(const Callback &c);
    static void push_event(std::string topic, const unsigned char *payload,
                           unsigned int length);
    static bool resolve_event();
};

class WARDuino {
   private:
    std::vector<Module *> modules = {};
    std::deque<uint8_t *> parsedInterrups = {};

    // factually volatile

    volatile bool interruptWrite;
    volatile bool interruptRead;
    bool interruptEven = true;
    uint8_t interruptLastChar;
    std::vector<uint8_t> interruptBuffer;
    long interruptSize;

   public:
    // vector, we expect few breakpoints
    std::set<uint8_t *> breakpoints = {};

    // Breakpoint to skip in the next interpretation step
    uint8_t *skipBreakpoint = nullptr;

    WARDuino();

    int run_module(Module *m);

    Module *load_module(uint8_t *bytes, uint32_t byte_count, Options options);

    void unload_module(Module *m);

    bool invoke(Module *m, uint32_t fidx);

    uint32_t get_export_fidx(Module *m, const char *name);

    void handleInterrupt(size_t len, uint8_t *buff);

    // breakpoints
    void addBreakpoint(uint8_t *loc);

    void delBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);

    // Get interrupt or NULL if none
    uint8_t *getInterrupt();
};

#endif
