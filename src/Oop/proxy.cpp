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
        m->warduino->program_state = PROXYhalt;
        m->warduino->debugger->sendProxyCallResult(m);
        return;
    }

    // push function to callstack
    m->warduino->interpreter->setup_call(m, rfc->fidx);
    // push proxy guard block to stack
    this->pushProxyGuard(m);

    m->warduino->program_state = PROXYrun;
}

RFC *Proxy::topRFC() { return this->calls->top(); }

void Proxy::returnResult(Module *m) {
    RFC *rfc = this->calls->top();

    // remove call from lifo queue
    this->calls->pop();

    if (!rfc->success) {
        // TODO exception msg
        WARDuino::instance()->debugger->channel->write(R"({"success":false})");
        return;
    }

    if (rfc->type->result_count == 0) {
        // reading result from stack
        WARDuino::instance()->debugger->channel->write(R"({"success":true})");
        return;
    }

    // send the result to the client
    rfc->result = &m->stack[m->sp];
    char *val = printValue(rfc->result);
    WARDuino::instance()->debugger->channel->write(R"({"success":true,%s})",
                                                   val);
    free(val);
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
    StackValue *args = callee->args;
    for (uint32_t i = 0; i < callee->type->param_count; i++)
        m->stack[++m->sp] = args[i];
}

void Proxy::pushProxyGuard(Module *m) {
    if (m == nullptr) {
        return;
    }
    auto *guard = (Block *)malloc(sizeof(struct Block));
    guard->block_type = 0xfe;  // 0xfe proxy guard
    m->warduino->interpreter->push_block(m, guard, m->sp);
}
