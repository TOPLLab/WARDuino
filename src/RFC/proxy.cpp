#include "proxy.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <queue>

#include "../Interpreter/instructions.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "proxy_supervisor.h"

// TODO tests with exceptions
////TODO test with many args proxy
////TODO test with no return proxy

unsigned short int sizeSerializationRFCallee(RFC *);
void arguments_copy(unsigned char *, StackValue *, uint32_t);

/*
 * Proxy methods
 */

Proxy::Proxy() {
    this->exceptionMsg = nullptr;
    this->excpMsgSize = 0;
    this->succes = true;
}

void Proxy::pushRFC(Module *m, RFC *rfc) {
    // push proxy guard block to stack
    this->pushProxyGuard(m);
    // push RFC arguments to stack
    this->setupCalleeArgs(m, rfc);
    // push function to stack
    setup_call(m, rfc->fidx);
    // keep RFC in queue
    this->calls->push(rfc);
}

void Proxy::returnResult(Module *m) {
    RFC *rfc = this->calls->top();
    // reading result from stack
    if (this->succes && rfc->type->result_count > 0) {
        rfc->result->value_type = m->stack[m->sp].value_type;
        rfc->result->value = m->stack[m->sp].value;
    } else if (!this->succes) {
        printf("some exception will be returned\n");
        // TODO exception msg
    }
    // remove call from lifo queue
    calls->pop();

    // send the result to the client
    struct SerializeData *rfc_result = this->serializeRFCallee(rfc);
    const char *data = (const char *)rfc_result->raw;
    WARDuino::instance()->debugger->channel->write(data);
}

struct SerializeData *Proxy::serializeRFCallee(RFC *callee) {
    const unsigned short serializationSize = sizeSerializationRFCallee(callee);
    auto *raw = new unsigned char[serializationSize];
    uint8_t suc = this->succes ? 1 : 0;

    memcpy(raw, &suc, sizeof(uint8_t));
    if (this->succes && callee->type->result_count > 0) {
        printf("serializeRFCallee: success value size=%u \n",
               sizeof_valuetype(callee->result->value_type));
        memcpy(raw + 1, &callee->result->value,
               sizeof_valuetype(callee->result->value_type));
    } else if (!this->succes) {
        printf("serializeRFCallee: serializing exception\n");
        memcpy(raw + 1, &this->excpMsgSize, sizeof(uint16_t));
        memcpy(raw + 1 + sizeof(uint16_t), this->exceptionMsg,
               this->excpMsgSize);
    }

    auto *ser = new struct SerializeData;
    ser->raw = raw;
    ser->size = serializationSize;
    return ser;
}

unsigned short int sizeSerializationRFCallee(RFC *callee) {
    if (!callee->success) return 1 + sizeof(uint16_t) + callee->exception_size;

    if (callee->type->result_count > 0)
        return 1 + sizeof_valuetype(callee->type->results[0]);
    else
        return 1;
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
                printf("arg %d: i32 value %" PRIu32 "\n", i,
                       args[i].value.uint32);
                break;
            }
            case F32: {
                memcpy(&args[i].value.f32, data, sizeof(float));
                data += sizeof(float);
                printf("arg %d: F32 value %.7f \n", i, args[i].value.f32);
                break;
            }
            case I64: {
                memcpy(&args[i].value.uint64, data, sizeof(uint64_t));
                data += sizeof(uint64_t);
                printf("arg %d: I64 value %" PRIu64 "\n", i,
                       args[i].value.uint64);
                break;
            }
            case F64: {
                memcpy(&args[i].value.f64, data, sizeof(double));
                data += sizeof(double);
                printf("arg %d: f64 value %.7f \n", i, args[i].value.f64);
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
