#pragma once

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>

#include "../Utils/macros.h"
#include "../WARDuino/internals.h"

// Size of memory load/store operations indexed by opcode - 0x28
extern uint32_t LOAD_SIZE[];
extern uint32_t LOAD_TYPES[];
extern uint32_t STORE_SIZE[];

class Interpreter {
   public:
    void push_block(Module *m, Block *block, int sp);

    virtual Block *pop_block(Module *m);

    virtual void setup_call(Module *m, uint32_t fidx);

    virtual bool interpret(Module *m, bool waiting = false);

    virtual void load(Module *m, uint32_t offset, uint32_t addr, int size,
                      uint8_t value_type, bool sign_extend);
    virtual bool store(Module *m, uint8_t type, uint32_t addr, int value_sp);

    static void report_overflow(Module *m, uint8_t *maddr);

    bool i_instr_block(Module *m, uint8_t *block_ptr);
    bool i_instr_loop(Module *m, uint8_t *block_ptr);
    bool i_instr_if(Module *m, uint8_t *block_ptr);
    bool i_instr_else(Module *m);
    bool i_instr_end(Module *m, bool *prog_done);
    bool i_instr_br(Module *m);
    bool i_instr_br_if(Module *m);
    bool i_instr_br_table(Module *m);
    bool i_instr_return(Module *m);
    bool i_instr_call(Module *m);
    bool i_instr_call_indirect(Module *m);
    bool i_instr_drop(Module *m);
    bool i_instr_select(Module *m);
    bool i_instr_get_local(Module *m);
    bool i_instr_set_local(Module *m);
    bool i_instr_tee_local(Module *m);
    bool i_instr_get_global(Module *m);
    bool i_instr_set_global(Module *m);
    bool i_instr_current_memory(Module *m);
    bool i_instr_grow_memory(Module *m);
    bool i_instr_mem_load(Module *m, uint8_t opcode);
    bool i_instr_mem_store(Module *m, uint8_t opcode);
    bool i_instr_const(Module *m, uint8_t opcode);
    bool i_instr_unary_u32(Module *m, uint8_t opcode);
    bool i_instr_math_u32(Module *m, uint8_t opcode);
    bool i_instr_math_u64(Module *m, uint8_t opcode);
    bool i_instr_math_f32(Module *m, uint8_t opcode);
    bool i_instr_math_f64(Module *m, uint8_t opcode);
    bool i_instr_unary_i32(Module *m, uint8_t opcode);
    bool i_instr_unary_i64(Module *m, uint8_t opcode);
    bool i_instr_unary_floating(Module *m, uint8_t opcode);
    bool i_instr_binary_i32(Module *m, uint8_t opcode);
    bool i_instr_binary_i64(Module *m, uint8_t opcode);
    bool i_instr_binary_f32(Module *m, uint8_t opcode);
    bool i_instr_binary_f64(Module *m, uint8_t opcode);
    bool i_instr_conversion(Module *m, uint8_t opcode);
    bool i_instr_callback(Module *m, uint8_t opcode);

    void register_primitive(const char *name, const Primitive f, Type *t) {
        PrimitiveEntry p = {};
        p.name = name;
        p.t = t;
        p.f = f;
        p.f_reverse = nullptr;
        p.f_serialize_state = nullptr;
        register_primitive(p);
    }

    void register_primitive(const PrimitiveEntry &entry) {
        dbg_info("installing primitive %s\n", entry.name);
        primitives[entry.name] = entry;
    }

    bool resolve_primitive(const char *symbol, Primitive *val) {
        debug("Resolve primitive %s\n", symbol);
        auto it = primitives.find(symbol);
        if (it != primitives.end()) {
            debug("FOUND PRIMITIVE\n");
            *val = it->second.f;
            return true;
        }
        FATAL("Could not find primitive %s \n", symbol);
        return false;  // unreachable
    }

    void restore_external_state(
        Module *m, const std::vector<IOStateElement> &external_state) {
        std::set<std::string> prim_names;
        for (uint32_t i = 0; i < m->import_count; i++) {
            prim_names.emplace(m->functions[i].import_field);
        }

        for (auto &[name, p] : primitives) {
            if (prim_names.find(name) != prim_names.end()) {
                if (p.f_reverse) {
                    dbg_info("Reversing state for primitive %s\n", p.name);
                    p.f_reverse(m, external_state);
                }
            }
        }
    }

    std::vector<IOStateElement *> get_io_state(Module *) const {
        std::vector<IOStateElement *> ioState;
        for (auto &[name, primitive] : primitives) {
            if (primitive.f_serialize_state) {
                primitive.f_serialize_state(ioState);
            }
        }
        return ioState;
    }

   private:
    std::unordered_map<std::string, PrimitiveEntry> primitives;
};
