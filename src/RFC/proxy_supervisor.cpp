#ifndef ARDUINO
#include "proxy_supervisor.h"

#include <netinet/in.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nlohmann/json.hpp>

#include "../Utils/macros.h"
#include "../Utils/util.h"

// TODO exception msg
const char SUCCESS[] = "";  // Empty denotes success
const char NO_HOST_ERR[] = "No host and port set";
const char CREATE_SOCK_ERR[] = "Could not create Socket";
const char INVALID_HOST[] = "Invalid host";
const char CONNECT_ERR[] = "Socket failed to connect";
const char WRITE_ERR[] = "ERROR writing to socket";
const char READ_ERR[] = "ERROR reading from socket";

bool is_success(const char *msg) {
    return (msg != nullptr) && (msg[0] == '\0');  // check if string is empty
}

bool continuing(pthread_mutex_t *mutex) {
    switch (pthread_mutex_trylock(mutex)) {
        case 0: /* if we got the lock, unlock and return false */
            pthread_mutex_unlock(mutex);
            return false;
        case EBUSY: /* return true if the mutex was locked */
        default:
            return true;
    }
}

void *readSocket(void *input) {
    // Print value received as argument:
    dbg_info("\n=== LISTENING TO SOCKET (in separate thread) ===\n");
    auto *supervisor = (ProxySupervisor *)input;
    supervisor->startPushDebuggerSocket();
    pthread_exit(nullptr);
}

Event *parseJSON(char *buff) {
    // TODO duplicate code in Debugger::handlePushedEvent
    nlohmann::basic_json<> parsed = nlohmann::json::parse(buff);
    printf("parseJSON: %s\n", parsed.dump().c_str());
    std::string payload = *parsed.find("payload");
    return new Event(*parsed.find("topic"), payload);
}

ProxySupervisor::ProxySupervisor(int socket, pthread_mutex_t *mutex) {
    printf("Started supervisor.\n");
    this->channel = new Channel(socket);
    this->mutex = mutex;

    pthread_create(&this->threadid, nullptr, readSocket, this);
}

void ProxySupervisor::startPushDebuggerSocket() {
    char _char;
    uint32_t buf_idx = 0;
    const uint32_t start_size = 1024;
    uint32_t current_size = start_size;
    char *buffer = (char *)malloc(start_size);

    printf("Started listening for events from proxy device.\n");
    while (continuing(this->mutex)) {
        if (this->channel->read(&_char, 1) != -1) {
            // increase buffer size if needed
            if (current_size <= (buf_idx + 1)) {
                char *new_buff = (char *)malloc(current_size + start_size);
                memcpy((void *)new_buff, (void *)buffer, current_size);
                free(buffer);
                buffer = new_buff;
                current_size += start_size;
                printf("increasing PushClient's buffer size to %d\n",
                       current_size);
            }
            buffer[buf_idx++] = _char;
            // manual null-termination is needed because parseJSON does not use
            // first len argument
            buffer[buf_idx] = '\0';
            try {
                Event *event = parseJSON(buffer);
                CallbackHandler::push_event(event);
                WARDuino::instance()->debugger->notifyPushedEvent();
                buf_idx = 0;
            } catch (const nlohmann::detail::parse_error &e) {
            }
        }
    }
}

bool ProxySupervisor::send(
    void *buffer, int size) {  // TODO buffer needs to be null terminated
    int n = this->channel->write(static_cast<const char *>(buffer));
    return n == size;
}

char *ProxySupervisor::readReply(short int amount) {
    char *buffer = new char[amount + 1];
    bzero(buffer, amount + 1);
    ssize_t n = this->channel->read(buffer, amount);
    if (n > 0) return buffer;

    delete[] buffer;
    return nullptr;
}

pthread_t ProxySupervisor::getThreadID() { return this->threadid; }

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

/*
 * returns the quantity of bytes needed to serialize a Proxy.
 * The size includes: Interrupt + Id of the function + parameters
 */
unsigned short int sizeSerializationRFC(const Type *type) {
    short unsigned int paramSize = 0;
    for (uint32_t i = 0; i < type->param_count; i++)
        paramSize += sizeof_valuetype(type->params[i]);

    return 1 + sizeof(uint32_t) + paramSize;
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

/*
 *
 * output:  1 byte  | 4 bytes | sizeof(arg1.value_type) bytes |
 * sizeof(arg2.value_type) bytes | ... interrupt | funID   | arg1.value
 *
 * Output is also transformed to hexa
 *
 */
struct SerializeData *ProxySupervisor::serializeRFC(RFC *callee) {
    const unsigned short serializationSize = sizeSerializationRFC(callee->type);
    auto *buffer = new unsigned char[serializationSize];

    // write to array: interrupt, function identifier and arguments
    const unsigned char interrupt = interruptProxyCall;
    memcpy(buffer, &interrupt, sizeof(unsigned char));
    memcpy(buffer + 1, &callee->fidx, sizeof(uint32_t));
    arguments_copy(buffer + 5, callee->args, callee->type->param_count);

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

void ProxySupervisor::deserializeRFCResult(RFC *rfc) {
    uint8_t *call_result = nullptr;
    while (call_result == nullptr) {
        call_result = (uint8_t *)this->readReply();
    }
    rfc->success = (uint8_t)call_result[0] == 1;

    if (!rfc->success) {
        uint16_t msg_size = 0;
        memcpy(&msg_size, call_result + 1, sizeof(uint16_t));
        if (msg_size > rfc->exception_size) {
            delete[] rfc->exception;
            rfc->exception = new char[msg_size];
            rfc->exception_size = msg_size;
        }
        memcpy(rfc->exception, call_result + 1 + sizeof(uint16_t), msg_size);
        delete[] call_result;
        return;
    }

    if (rfc->type->result_count == 0) {
        delete[] call_result;
        return;
    }

    rfc->result->value.uint64 = 0;
    switch (rfc->result->value_type) {
        case I32:
            memcpy(&rfc->result->value.uint32, call_result + 1,
                   sizeof(uint32_t));
            dbg_info("deserialized U32 %" PRIu32 "\n", result->value.uint32);
            break;
        case F32:
            memcpy(&rfc->result->value.f32, call_result + 1, sizeof(float));
            dbg_info("deserialized f32 %f \n", result->value.f32);
            break;
        case I64:
            memcpy(&rfc->result->value.uint64, call_result + 1,
                   sizeof(uint64_t));
            dbg_info("deserialized I64 %" PRIu64 "\n", result->value.uint64);
            break;
        case F64:
            memcpy(&rfc->result->value.f64, call_result + 1, sizeof(double));
            dbg_info("deserialized f32 %f \n", result->value.f64);
            break;
        default:
            FATAL("Deserialization RFCResult\n");
    }
    delete[] call_result;
}

bool ProxySupervisor::call(RFC *callee) {
    struct SerializeData *rfc_request = this->serializeRFC(callee);

    bool sent = this->send((void *)rfc_request->raw, rfc_request->size);
    if (!sent) {
        callee->success = false;

        delete[] rfc_request->raw;
        delete rfc_request;
        dbg_trace("Sending RFC: FAILED\n");
        return false;
    }
    // Fetch new callback mapping
    // convert message to hex TODO: move to proxyserver
    char cmdBuffer[10] = "";
    int cmdBufferLen = 0;
    sprintf(cmdBuffer, "%x\n%n", interruptDUMPCallbackmapping, &cmdBufferLen);
    WARDuino::instance()->debugger->supervisor->send(cmdBuffer, cmdBufferLen);
    this->deserializeRFCResult(callee);
    return true;
}
#endif
