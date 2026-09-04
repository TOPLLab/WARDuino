#include "proxy.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "../Interpreter/instructions.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"

// TODO tests with exceptions
////TODO test with many args proxy
////TODO test with no return proxy

void arguments_copy(unsigned char *, StackValue *, uint32_t);
char *printValue(StackValue *v);
/*
 * Proxy methods
 */

Proxy::Proxy() = default;

void Proxy::pushRFC(Module *m, RFC *rfc) {
    // keep RFC in queue
    this->calls->push(rfc);

    // push RFC arguments to stack
    this->setupCalleeArgs(m, rfc);

    if (rfc->fidx < m->import_count) {
        // execute primitives directly
        ((Primitive)m->functions[rfc->fidx].func_ptr)(m);
        // send result directly
        m->warduino->program_state = debug_State_STATE_PROXY_HALT;
        m->warduino->debugger->send_proxy_call_result(m);
        return;
    }

    // push function to callstack
    m->warduino->interpreter->setup_call(m, rfc->fidx);
    // push proxy guard block to stack
    this->pushProxyGuard(m);

    m->warduino->program_state = debug_State_STATE_PROXY_RUN;
}

RFC *Proxy::topRFC() { return this->calls->top(); }

RFC *Proxy::returnResult(Module *m) {
    (void)m;
    if (this->calls->empty()) return nullptr;
    RFC *rfc = this->calls->top();
    this->calls->pop();
    return rfc;
}

char *printValue(StackValue *v) {
    char *buff = (char *)malloc(256);
    switch (v->value_type) {
        case I32:
            snprintf(buff, 255, R"("type":%d,"value":%)" PRIi32, I32,
                     v->value.uint32);
            break;
        case I64:
            snprintf(buff, 255, R"("type":%d,"value":%)" PRIi64, I64,
                     v->value.uint64);
            break;
        case F32:
            snprintf(buff, 255, R"("type":%d,"value":%.7f)", F32, v->value.f32);
            break;
        case F64:
            snprintf(buff, 255, R"("type":%d,"value":%.7f)", F64, v->value.f64);
            break;
        default:
            snprintf(buff, 255, R"("type":%02x,"value":%)" PRIx64,
                     v->value_type, v->value.uint64);
    }
    return buff;
}

StackValue *Proxy::readRFCArgs(Block *func, uint8_t *data) {
    return readArgs(*func->type, data);
}

void Proxy::setupCalleeArgs(Module *m, RFC *callee) {
    // adding arguments to the stack
    ExecutionContext *ectx = m->warduino->execution_context;
    StackValue *args = callee->args;
    for (uint32_t i = 0; i < callee->type->param_count; i++)
        ectx->stack[++ectx->sp] = args[i];
}

void Proxy::pushProxyGuard(Module *m) {
    if (m == nullptr) {
        return;
    }
    auto *guard = (Block *)malloc(sizeof(struct Block));
    guard->block_type = 0xfe;  // 0xfe proxy guard
    ExecutionContext *ectx = m->warduino->execution_context;
    m->warduino->interpreter->push_block(m, guard, ectx->sp);
}
