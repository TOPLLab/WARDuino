#pragma once
#include <functional>
#include <vector>

#include "../../src/WARDuino.h"

inline std::vector<uint8_t *> find_prim_calls(
    const Module *m, const std::function<bool(Block *)> &cond,
    const bool after = false) {
    std::vector<uint8_t *> call_sites;
    for (size_t i = m->import_count; i < m->function_count; i++) {
        Block *func = &m->functions[i];
        uint8_t *pc = func->start_ptr;
        while (pc < func->end_ptr) {
            uint8_t opcode = *pc;
            uint8_t *instruction_start_pc = pc;
            if (opcode == 0x10) {  // TODO: Support other call instructions
                pc++;
                uint32_t fidx = read_LEB_32(&pc);
                if (fidx < m->import_count) {
                    const char *module_name = m->functions[fidx].import_module;
                    if (!strcmp(module_name, "env") &&
                        cond(&m->functions[fidx])) {
                        if (!after) {
                            call_sites.push_back(instruction_start_pc);
                        } else {
                            // NOTE: Only works because this is a primitive
                            // call, if it was a regular call the vm would jump
                            // into a function and not just to the instruction
                            // after the call instruction.
                            call_sites.push_back(pc);
                        }
                    }
                }
                continue;
            }
            skip_immediates(&pc);
        }
    }
    return call_sites;
}

inline std::vector<uint8_t *> find_choice_points(const Module *m,
                                                 const bool after = false) {
    return find_prim_calls(
        m, [](const Block *prim) { return prim->type->result_count > 0; },
        after);
}

inline std::vector<uint8_t *> find_pc_before_primitive_calls(const Module *m) {
    return find_prim_calls(
        m, []([[maybe_unused]] const Block *prim) { return true; });
}

inline std::vector<uint8_t *> find_pc_after_primitive_calls(const Module *m) {
    return find_prim_calls(
        m, []([[maybe_unused]] const Block *prim) { return true; }, true);
}
