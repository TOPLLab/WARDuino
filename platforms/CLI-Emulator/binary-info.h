#pragma once
#include <functional>
#include <string>
#include <vector>

#include "../../src/WARDuino.h"

inline std::vector<uint8_t *> find_calls(
    const Module *m, const std::function<bool(std::string)> &cond,
    const bool after = false) {
    std::vector<uint8_t *> call_sites;
    for (size_t i = m->import_count; i < m->function_count; i++) {
        Block *func = &m->functions[i];
        uint8_t *pc = func->start_ptr;
        while (pc < func->end_ptr) {
            uint8_t opcode = *pc;
            uint8_t *instruction_start_pc = pc;
            if (opcode == 0x10) {
                pc++;
                uint32_t fidx = read_LEB_32(&pc);
                if (fidx < m->import_count) {
                    const char *module_name = m->functions[fidx].import_module;
                    const char *field_name = m->functions[fidx].import_field;
                    if (!strcmp(module_name, "env") && cond(field_name)) {
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
    std::set<std::string> symbolic_primitives = {
        "chip_digital_read",
        "chip_analog_read",
        "color_sensor",
    };
    return find_calls(
        m,
        [symbolic_primitives](const std::string &field_name) {
            return symbolic_primitives.find(field_name) !=
                   symbolic_primitives.end();
        },
        after);
}

inline std::vector<uint8_t *> find_pc_before_primitive_calls(const Module *m) {
    return find_calls(
        m, []([[maybe_unused]] const std::string &x) { return true; });
}

inline std::vector<uint8_t *> find_pc_after_primitive_calls(const Module *m) {
    return find_calls(
        m, []([[maybe_unused]] const std::string &x) { return true; }, true);
}
