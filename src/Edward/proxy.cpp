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

    // push proxy guard block to stack
    this->pushProxyGuard(m);
    // push function to stack
    setup_call(m, rfc->fidx);

    m->warduino->program_state = PROXYrun;
}

RFC *Proxy::topRFC() { return this->calls->top(); }

void Proxy::returnResult(Module *m) {
    RFC *rfc = this->calls->top();

    // reading result from stack
    if (rfc->success && rfc->type->result_count > 0) {
        rfc->result = &m->stack[m->sp];
    } else if (!rfc->success) {
        printf("some exception will be returned\n");
        // TODO exception msg
    }
    // remove call from lifo queue
    calls->pop();

    // send the result to the client
    char *val = printValue(rfc->result);
    WARDuino::instance()->debugger->channel->write(
        R"({"success":%s,%s})", rfc->success ? "true" : "false", val);
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
            snprintf(buff, 255, R"("type":"%02x","value":"%)" PRIx64 "\"",
                     v->value_type, v->value.uint64);
    }
    return buff;
}

StackValue *Proxy::readRFCArgs(Block *func, uint8_t *data) {
    if (func->type->param_count == 0) {
        printf("ProxyFunc %" PRIu32 "takes no arg\n", func->fidx);
        return nullptr;
    }

    auto *args = new StackValue[func->type->param_count];
    uint32_t *params = func->type->params;
    for (uint32_t i = 0; i < func->type->param_count; i++) {
        args[i].value.uint64 = 0;  // init whole union to 0
        args[i].value_type = params[i];

        switch (params[i]) {
            case I32: {
                memcpy(&args[i].value.uint32, data, sizeof(uint32_t));
                data += sizeof(uint32_t);
                printf("arg %" PRIu32 ": i32 value %" PRIu32 "\n", i,
                       args[i].value.uint32);
                break;
            }
            case F32: {
                memcpy(&args[i].value.f32, data, sizeof(float));
                data += sizeof(float);
                printf("arg %" PRIu32 ": F32 value %.7f \n", i,
                       args[i].value.f32);
                break;
            }
            case I64: {
                memcpy(&args[i].value.uint64, data, sizeof(uint64_t));
                data += sizeof(uint64_t);
                printf("arg %" PRIu32 ": I64 value %" PRIu64 "\n", i,
                       args[i].value.uint64);
                break;
            }
            case F64: {
                memcpy(&args[i].value.f64, data, sizeof(double));
                data += sizeof(double);
                printf("arg %" PRIu32 ": f64 value %.7f \n", i,
                       args[i].value.f64);
                break;
            }
            default: {
                FATAL("incorrect argument type: %" PRIu32 "\n", params[i]);
                break;
            }
        }
    }
    return args;
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
    auto *guard = new Block();
    guard->block_type = 255;  // 0xfe proxy guard
    push_block(m, guard, m->sp);
}
