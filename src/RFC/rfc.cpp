#include "rfc.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <queue>

#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "proxy_server.h"
#ifdef ARDUINO
#include "SocketServer.h"
#endif

// TODO tests with exceptions
////TODO test with many args proxy
////TODO test with no return proxy

unsigned short int sizeof_valuetype(uint32_t);
unsigned short int sizeSerializationRFC(const Type *);
unsigned short int sizeSerializationRFCallee(RFC *);
void arguments_copy(unsigned char *, StackValue *, uint32_t);

std::map<uint32_t, RFC *> functions;
std::queue<RFC *> callees;

/*
 * Proxy Manager Client Side
 */

RFC *RFC::registerRFC(uint32_t t_fid, Type *t_type) {
    RFC *rfc = new RFC(t_fid, t_type);
    functions[t_fid] = rfc;
    return rfc;
}

void RFC::unregisterRFC(uint32_t fid) {
    auto it = functions.find(fid);
    if (it != functions.end()) functions.erase(it);
}

void RFC::clearRFCs() {
    std::map<uint32_t, RFC *>::iterator it;
    for (it = functions.begin(); it != functions.end(); it++) delete it->second;
    functions.clear();
}

bool RFC::isRFC(uint32_t fid) {
    auto it = functions.find(fid);
    return it != functions.end();
}

RFC *RFC::getRFC(uint32_t fid) {
    auto it = functions.find(fid);
    return it->second;
}

/*
 * Proxy Manager Server Side
 */

RFC *RFC::registerRFCallee(uint32_t t_fid, Type *t_type, StackValue *t_args,
                           ExecutionState *t_executionState) {
    RFC *rfc = new RFC(t_fid, t_type, t_args, t_executionState);
    callees.push(rfc);
    return rfc;
}

bool RFC::hasRFCallee() { return !callees.empty(); }

RFC *RFC::currentCallee() { return callees.front(); }

void RFC::removeRFCallee() { callees.pop(); }

/*
 * RFC methods
 */

RFC::RFC(uint32_t t_fid, Type *t_type, StackValue *t_args,
         ExecutionState *t_exState)
    : fid(t_fid), args(t_args), type(t_type), executionState(t_exState) {
    this->exceptionMsg = nullptr;
    this->excpMsgSize = 0;
    this->succes = true;
    this->result = nullptr;
    if (t_type->result_count > 0) {
        this->result = new StackValue;
        this->result->value_type = t_type->results[0];
    }
}
#ifdef ARDUINO
void RFC::returnResult(Module *m) {
    // reading result from stack
    if (this->succes && this->type->result_count > 0) {
        this->result->value_type = m->stack[m->sp].value_type;
        this->result->value = m->stack[m->sp].value;
    } else if (!this->succes) {
        printf("some exeception will be returned\n");
        // TODO exception msg
    }

    // returning the result to the client
    struct SerializeData *rfc_result = this->serializeRFCallee();
    const char *data = (const char *)rfc_result->raw;
    size_t data_size = (size_t)rfc_result->size;
    SocketServer *server = SocketServer::getServer();
    server->write2Client(server->pullClient, data, data_size);
}
#endif

void RFC::restoreExecutionState(Module *m, RunningState *program_state) const {
    // restoring the original execution state
    *program_state = this->executionState->program_state;
    m->csp = this->executionState->csp;
    m->sp = this->executionState->sp;
    m->pc_ptr = this->executionState->pc_ptr;
}

bool RFC::callCompleted(Module *m) const {
    return !this->succes || this->executionState->csp == m->csp;
}

/*
 *
 * output:  1 byte  | 4 bytes | sizeof(arg1.value_type) bytes |
 * sizeof(arg2.value_type) bytes | ... interrupt | funID   | arg1.value
 *
 * Output is also transformed to hexa
 *
 */

struct RFC::SerializeData *RFC::serializeRFC() {
    const unsigned short serializationSize = sizeSerializationRFC(this->type);
    auto *buffer = new unsigned char[serializationSize];

    // write to array: interrupt, function identifier and arguments
    const unsigned char interrupt = interruptProxyCall;
    memcpy(buffer, &interrupt, sizeof(unsigned char));
    memcpy(buffer + 1, &fid, sizeof(uint32_t));
    arguments_copy(buffer + 5, args, type->param_count);

    // array as hexa
    const uint32_t hexa_size = serializationSize * 2;
    auto *hexa =
        new unsigned char[hexa_size + 2];  //+2 for '\n' and '0' termination
    chars_as_hexa(hexa, buffer, serializationSize);
    hexa[hexa_size] = '\n';
    hexa[hexa_size + 1] = '\0';  // TODO remove zero termination and +2 above

    delete[] buffer;
    auto *ser = new SerializeData;
    ser->size = hexa_size + 1;
    ser->raw = hexa;
    return ser;
}

struct RFC::SerializeData *RFC::serializeRFCallee() {
    const unsigned short serializationSize = sizeSerializationRFCallee(this);
    auto *raw = new unsigned char[serializationSize];
    uint8_t suc = this->succes ? 1 : 0;

    memcpy(raw, &suc, sizeof(uint8_t));
    if (this->succes && this->type->result_count > 0) {
        printf("serializeRFCallee: success value size=%u \n",
               sizeof_valuetype(this->result->value_type));
        memcpy(raw + 1, &this->result->value,
               sizeof_valuetype(this->result->value_type));
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

/*
 * returns the quantity of bytes needed to serialize a RFC.
 * The size includes: Interrupt + Id of the function + parameters
 */
unsigned short int sizeSerializationRFC(const Type *type) {
    short unsigned int paramSize = 0;
    for (uint32_t i = 0; i < type->param_count; i++)
        paramSize += sizeof_valuetype(type->params[i]);

    return 1 + sizeof(uint32_t) + paramSize;
}

unsigned short int sizeSerializationRFCallee(RFC *callee) {
    if (!callee->succes) return 1 + sizeof(uint16_t) + callee->excpMsgSize;

    if (callee->type->result_count > 0)
        return 1 + sizeof_valuetype(callee->type->results[0]);
    else
        return 1;
}

unsigned short int sizeof_valuetype(uint32_t vt) {
    switch (vt) {
        case I32:
            return 4;
        case I64:
            return 8;
        case F32:
            return sizeof(float);
        default:
            return sizeof(double);
    }
}

void arguments_copy(unsigned char *dest, StackValue *args,
                    uint32_t param_count) {
    uint32_t offset = 0;
    for (uint32_t i = 0; i < param_count; i++) {
        switch (args[i].value_type) {
            case I32: {
                memcpy(dest + offset, &args[i].value.uint32, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                break;
            }
            case F32: {
                memcpy(dest + offset, &args[i].value.f32, sizeof(float));
                offset += sizeof(float);
                break;
            }
            case I64: {
                memcpy(dest + offset, &args[i].value.uint64, sizeof(uint64_t));
                offset += sizeof(uint64_t);
                break;
            }
            case F64: {
                memcpy(dest + offset, &args[i].value.f64, sizeof(double));
                offset += sizeof(double);
                break;
            }
            default:
                FATAL("incorrect StackValue type\n");
                break;
        }
    }
}

void RFC::deserializeRFCResult() {
    ProxyServer *host = ProxyServer::getServer();

    auto *call_result = (uint8_t *)host->readReply();
    this->succes = (uint8_t)call_result[0] == 1;

    if (!this->succes) {
        uint16_t msg_size = 0;
        memcpy(&msg_size, call_result + 1, sizeof(uint16_t));
        if (msg_size > this->excpMsgSize) {
            delete[] this->exceptionMsg;
            this->exceptionMsg = new char[msg_size];
            this->excpMsgSize = msg_size;
        }
        memcpy(this->exceptionMsg, call_result + 1 + sizeof(uint16_t),
               msg_size);
        delete[] call_result;
        return;
    }

    if (this->type->result_count == 0) {
        delete[] call_result;
        return;
    }

    this->result->value.uint64 = 0;
    switch (this->result->value_type) {
        case I32:
            memcpy(&result->value.uint32, call_result + 1, sizeof(uint32_t));
            dbg_info("deserialized U32 %" PRIu32 "\n", result->value.uint32);
            break;
        case F32:
            memcpy(&result->value.f32, call_result + 1, sizeof(float));
            dbg_info("deserialized f32 %f \n", result->value.f32);
            break;
        case I64:
            memcpy(&result->value.uint64, call_result + 1, sizeof(uint64_t));
            dbg_info("deserialized I64 %" PRIu64 "\n", result->value.uint64);
            break;
        case F64:
            memcpy(&result->value.f64, call_result + 1, sizeof(double));
            dbg_info("deserialized f32 %f \n", result->value.f64);
            break;
        default:
            FATAL("Deserialization RFCResult\n");
    }
    delete[] call_result;

    // Retreive new callbackmapping
    std::string message = std::to_string(interruptDUMPCallbackmapping) + "\n";
    ProxyServer::getServer()->send((char *)message.c_str(),
                                   (int)message.length());
}

void RFC::call(StackValue *arguments) {
    this->args = arguments;
    struct SerializeData *rfc_request = this->serializeRFC();

    ProxyServer *host = ProxyServer::getServer();
    printf("making the RFC call\n");
    bool sent = host->send((void *)rfc_request->raw, rfc_request->size);
    if (!sent) {
        this->succes = false;
        this->exceptionMsg = host->exceptionMsg;

        delete[] rfc_request->raw;
        delete rfc_request;
        return;
    }
    this->deserializeRFCResult();
}

void RFC::registerRFCs(Module *m, uint8_t **data) {
    printf("registering_rfc_functions\n");
    RFC::clearRFCs();
    uint32_t amount_funcs = read_B32(data);
    printf("funcs_total %" PRIu32 "\n", amount_funcs);
    for (uint32_t i = 0; i < amount_funcs; i++) {
        uint32_t fid = read_B32(data);
        printf("registering fid=%" PRIu32 "\n", fid);
        Type *type = (m->functions[fid]).type;
        RFC::registerRFC(fid, type);
    }
}

StackValue *RFC::readRFCArgs(Block *func, uint8_t *data) {
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

void RFC::setupCalleeArgs(Module *m, RFC *callee) {
    // adding arguments to the stack
    StackValue *args = callee->args;
    for (uint32_t i = 0; i < callee->type->param_count; i++)
        m->stack[++m->sp] = args[i];
}
