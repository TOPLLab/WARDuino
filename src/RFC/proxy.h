#pragma once

#include <unistd.h>

#include <cstdint>
#include <stack>

#include "RFC.h"
struct Module;
struct Block;

class Proxy {
   private:
    std::stack<RFC *> *calls = new std::stack<RFC *>();  // lifo queue

    struct SerializeData *serializeRFCallee(RFC *callee);
    void setupCalleeArgs(Module *m, RFC *callee);
    void pushProxyGuard(Module *m);

   public:
    Proxy();

    void pushRFC(Module *m, RFC *rfc);
    RFC *topRFC();
    void returnResult(Module *m);

    // Server side ( arduino side )
    static StackValue *readRFCArgs(Block *func, uint8_t *data);
};
