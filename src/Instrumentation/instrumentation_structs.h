#pragma once
#include "../Instrumentation/hook.h"
#include "../WARDuino/structs.h"

enum HookMoment { HookBefore = 0x01, HookAfter = 0x02, HookAround = 0x03 };

typedef struct HooksPrimitiveFunc {
    uint32_t func_idx;          // func for which the around hook is registered
    Primitive original_func{};  // original function
    Hook *hook{};               // hooks to perform instead of original_func
} HooksPrimitiveFunc;

typedef struct HooksWasmAddr {
    uint32_t address{};         // wasm address that needs to be intercepted
    uint8_t original_opcode{};  // original opcode
    Hook *hook{};               // hooks to perform on address
} HooksWasmAddr;

enum HookAroundKind { AroundInstruction = 0x00, AroundFunctionBody = 0x01 };

typedef struct HookAroundResultRequest {
    uint32_t id{};
    uint32_t addr{};
    HookAroundKind kind{};
    uint32_t nr_args{};
    bool has_result{};
    StackValue result{};
} HookAroundResultRequest;
