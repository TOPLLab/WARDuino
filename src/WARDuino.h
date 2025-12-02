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

void skip_immediates(uint8_t **pos);

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

    uint32_t get_export_global_idx(Module *m, const char *name);

    void handleInterrupt(size_t len, uint8_t *buff) const;

    void instantiate_module(Module *m, uint8_t *bytes, uint32_t byte_count);

    void free_module_state(Module *m);
};
