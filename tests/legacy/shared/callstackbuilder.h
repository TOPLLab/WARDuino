#pragma once
#include "../../src/WARDuino.h"

class CallstackBuilder {
   private:
    void pushBlock(Block* b, int sp);

    Module* m{};

   public:
    CallstackBuilder(Module* wasm_module);

    void pushFunctionCall(uint32_t fidx);

    void pushGuard(uint8_t guard_type);
};