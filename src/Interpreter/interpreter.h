#pragma once

#include <cstdint>

#include "../WARDuino/internals.h"

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

   protected:
   private:
};
