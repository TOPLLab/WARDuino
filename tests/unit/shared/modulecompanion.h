#pragma once
#include "../../src/WARDuino.h"

class ModuleCompanion {
   private:
    Module* m{};

   public:
    ModuleCompanion(Module* wasm_module);

    Block* getMainFunction();
    Block* firstBlock(uint8_t type);
};