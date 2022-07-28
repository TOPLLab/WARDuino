#pragma once

#include <unistd.h>

#include <cstdint>

#include "RFC.h"

class Proxy {
   private:
    struct SerializeData *serializeRFCallee(RFC *callee);

    void setupCalleeArgs(Module *m, RFC *callee);
    void pushProxyGuard(Module *m);

   public:
    StackValue *result;
    bool succes;
    char *exceptionMsg;
    uint16_t excpMsgSize;

    Proxy();

    void pushRFC(Module *m, RFC *rfc);
    void returnResult(Module *m, RFC *rfc);

    // Server side ( arduino side )
    static StackValue *readRFCArgs(Block *func, uint8_t *data);
};
