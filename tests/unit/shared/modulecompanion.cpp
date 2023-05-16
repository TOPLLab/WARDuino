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