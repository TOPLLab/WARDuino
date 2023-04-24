#pragma once

#include <unistd.h>

#include <cstdint>
#include <stack>

#include "../Debug/debug_structs.h"
#include "RFC.h"
struct Module;
struct Block;

class Proxy {
   private:
    std::stack<RFC *> *calls = new std::stack<RFC *>();  // lifo queue
    std::stack<RunningState> *runStates =
        new std::stack<RunningState>();  // lifo queue

    void setupCalleeArgs(Module *m, RFC *callee);
    void pushProxyGuard(Module *m);

   public:
    Proxy();

    void pushRFC(Module *m, RFC *rfc, RunningState runState);
    RFC *topRFC();
    void returnResult(Module *m);

    // Server side ( arduino side )
    static StackValue *readRFCArgs(Block *func, uint8_t *data);
};
