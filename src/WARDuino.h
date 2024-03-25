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
#include "Interpreter/interpreter.h"
#include "WARDuino/internals.h"

// Constants
#define WA_MAGIC 0x6d736100
#define WA_VERSION 0x01

#define PAGE_SIZE 0x10000      // 65536 bytes TODO
#define STACK_SIZE 0x100       // 65536
#define BLOCKSTACK_SIZE 0x100  // 4096
#define CALLSTACK_SIZE 0x100   // 4096
#define BR_TABLE_SIZE 0x100    // 65536

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

    void run_init_expr(Module *m, uint8_t type, uint8_t **pc);

   public:
    Debugger *debugger;
    Interpreter *interpreter;
    RunningState program_state = WARDUINOrun;

    static WARDuino *instance();

    void setInterpreter(Interpreter *interpreter);

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
