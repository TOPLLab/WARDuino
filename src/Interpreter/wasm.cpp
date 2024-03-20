#include "./wasm.h"

#include "../Utils/macros.h"
#include "../WARDuino.h"
uint32_t STORE_SIZE[] = {4, 8, 4, 8, 1, 2, 1, 2, 4};

void report_overflow(Module *m, uint8_t *maddr) {
    dbg_warn("memory start: %p, memory end: %p, maddr: %p\n", m->memory.bytes,
             m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE, maddr);
    sprintf(exception, "out of bounds memory access");
}

bool store(Module *m, uint8_t type, uint32_t addr, StackValue &sval) {
    uint8_t *maddr, *mem_end;
    uint32_t size = STORE_SIZE[abs(type - I32)];
    bool overflow = false;

    maddr = m->memory.bytes + addr;
    if (maddr < m->memory.bytes) {
        overflow = true;
    }
    mem_end = m->memory.bytes + m->memory.pages * (uint32_t)PAGE_SIZE;
    if (maddr + size > mem_end) {
        overflow = true;
    }

    if (!m->options.disable_memory_bounds) {
        if (overflow) {
            report_overflow(m, maddr);
            return false;
        }
    }

    memcpy(maddr, &sval.value, size);
    return true;
}
