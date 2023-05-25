#include "modulecompanion.h"

#define UNDEF (uint32_t)(-1)

ModuleCompanion::ModuleCompanion(Module *wasm_module) : m{wasm_module} {}

Block *ModuleCompanion::getMainFunction() {
    WARDuino *w = this->m->warduino;
    uint32_t fidx = w->get_export_fidx(m, "main");
    if (fidx == UNDEF) fidx = w->get_export_fidx(m, "Main");
    if (fidx == UNDEF) fidx = w->get_export_fidx(m, "_main");
    if (fidx == UNDEF) fidx = w->get_export_fidx(m, "_Main");
    if (fidx == UNDEF)
        return nullptr;
    else
        return &this->m->functions[fidx];
}

Block *ModuleCompanion::firstBlock(uint8_t type) {
    auto find =
        std::find_if(std::begin(m->block_lookup), std::end(m->block_lookup),
                     [&](const std::pair<uint8_t *, Block *> &pair) {
                         return pair.second->block_type == type;
                     });
    Block *block = nullptr;
    if (find != std::end(m->block_lookup)) {
        block = find->second;
    }
    return block;
}