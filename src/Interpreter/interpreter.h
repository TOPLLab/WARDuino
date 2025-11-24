#pragma once

#include <cstdint>
#include <cstring>
#include <set>

#include "../WARDuino/internals.h"
// #include "../Utils/macros.h"

class Interpreter {
   public:
    /**
     * Push a new frame on to the call stack
     * @param m module
     * @param block
     * @param sp stack pointer to return to
     */
    void push_block(Module *m, Block *block, int sp);

    /**
     * Restore the top frame from the call stack of a module.
     *
     * Sets stack pointer and call stack pointer, verifies type of return
     *
     * @param m module
     * @return block that was on top of the stack (the one that we just returned
     * to)
     */
    Block *pop_block(Module *m);

    /**
     * Setup a function
     *
     * Push params and locals on the stack and save a call frame on the call
     * stack Sets new pc value for the start of the function
     *
     * @param m module to work on
     * @param fidx function id (index in m->functions)
     */
    void setup_call(Module *m, uint32_t fidx);

    /**
     * Start interpretation of the module that has a call set up for it.
     * See #setup_call
     * @param m module that has been set up for a call
     * @param return_exception whether to save the exception method or not
     * @return true if the interpretation ended as expected
     */
    bool interpret(Module *m, bool waiting = false);

    /* Stateful operations
     * ************************************************************************/

    bool store(Module *m, uint8_t type, uint32_t addr, StackValue &sval);

    bool load(Module *m, uint8_t type, uint32_t addr, uint32_t offset);

    static void report_overflow(Module *m, uint8_t *maddr);

    void register_primitive(const PrimitiveEntry &entry) {
        primitives.push_back(entry);
    }

    //------------------------------------------------------
    // resolving the primitives
    //------------------------------------------------------
    // ReSharper disable once CppDFAConstantFunctionResult
    bool resolve_primitive(const char *symbol, Primitive *val) {
        // debug("Resolve primitives (%d) for %s  \n", ALL_PRIMITIVES, symbol);

        for (auto &primitive : primitives) {
            //        printf("Checking %s = %s  \n", symbol, primitive.name);
            if (!strcmp(symbol, primitive.name)) {
                // debug("FOUND PRIMITIVE\n");
                *val = primitive.f;
                return true;
            }
        }
        // FATAL("Could not find primitive %s \n", symbol);
        return false;
        // return false; // unreachable
    }

    //------------------------------------------------------
    // Restore external state when restoring a snapshot
    //------------------------------------------------------
    void restore_external_state(
        Module *m, const std::vector<IOStateElement> &external_state) {
        std::set<std::string> prim_names;
        for (uint32_t i = 0; i < m->import_count; i++) {
            prim_names.emplace(m->functions[i].import_field);
        }

        for (PrimitiveEntry &p : primitives) {
            if (prim_names.find(p.name) != prim_names.end()) {
                if (p.f_reverse) {
                    printf("Reversing state for primitive %s\n", p.name);
                    p.f_reverse(m, external_state);
                }
            }
        }
    }

    std::vector<IOStateElement *> get_io_state(Module *) const {
        std::vector<IOStateElement *> ioState;
        for (auto &primitive : primitives) {
            if (primitive.f_serialize_state) {
                primitive.f_serialize_state(ioState);
            }
        }
        return ioState;
    }

   protected:
   private:
    std::vector<PrimitiveEntry> primitives;
};
