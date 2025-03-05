#include "proxy_supervisor.h"

#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef ARDUINO
#include <nlohmann/json.hpp>
#else
#include "../../lib/json/single_include/nlohmann/json.hpp"
#endif

#include "../Primitives/primitives.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/CallbackHandler.h"

// TODO exception msg
// const char SUCCESS[] = "";  // Empty denotes success
const char NO_HOST_ERR[] = "No host and port set";
const char CREATE_SOCK_ERR[] = "Could not create Socket";
const char INVALID_HOST[] = "Invalid host";
const char CONNECT_ERR[] = "Socket failed to connect";
const char WRITE_ERR[] = "ERROR writing to socket";
const char READ_ERR[] = "ERROR reading from socket";

bool is_success(const char *msg) {
    return (msg != nullptr) && (msg[0] == '\0');  // check if string is empty
}

bool continuing(warduino::mutex *mutex) {
    if (mutex->try_lock()) {
        /* if we got the lock, unlock and return false */
        mutex->unlock();
        return false;
    }
    /* return true if the mutex was locked */
    return true;
}

void runSupervisor(ProxySupervisor *supervisor) {
    // Print value received as argument:
    dbg_info("\n=== LISTENING TO SOCKET (in separate thread) ===\n");
    supervisor->listenToSocket();
}

Event *parseJSON(char *buff) {
    // TODO duplicate code in Debugger::handlePushedEvent
    nlohmann::basic_json<> parsed = nlohmann::json::parse(buff);
    printf("parseJSON: %s\n", parsed.dump().c_str());
    std::string payload = *parsed.find("payload");
    return new Event(*parsed.find("topic"), payload);
}

ProxySupervisor::ProxySupervisor(Channel *duplex, warduino::mutex *mutex)
    : Debugger(duplex) {
    debug("Starting supervisor.\n");
    this->mutex = mutex;
    this->thread = warduino::thread(runSupervisor, this);
    this->proxyResult = nullptr;
}

bool isEvent(nlohmann::basic_json<> parsed) {
    return parsed.find("topic") != parsed.end();
}

bool isReply(nlohmann::basic_json<> parsed) {
    return parsed.find("success") != parsed.end();
}

void ProxySupervisor::listenToSocket() {
    uint8_t message[1024] = {0};
    ssize_t readAmount;

    this->channel->open();

    dbg_info("Proxy supervisor listening to remote device...\n");
    while (continuing(this->mutex)) {
        readAmount = this->channel->read(message, 1024);
        if (readAmount == -1) {
            printf("Proxy supervisor shutting down.\n");
            exit(-1);
        }
        if (readAmount > 0) {
            std::string msg_str = std::string(reinterpret_cast<char *>(message));
            //printf("msg_str = \"%s\"\n", (char*) message);
            while (!msg_str.empty()) {
                auto first_msg = msg_str.substr(0, msg_str.find('\n'));
                msg_str = msg_str.substr(msg_str.find('\n') + 1);
                //printf("first_msg = \"%s\"\n", first_msg.c_str());

                try {
                    nlohmann::basic_json<> parsed = nlohmann::json::parse(first_msg);
                    //printf("parseJSON: %s\n", parsed.dump().c_str());

                    if (isEvent(parsed)) {
                        CallbackHandler::push_event(new Event(
                            *parsed.find("topic"), *parsed.find("payload")));
                        WARDuino::instance()->debugger->notifyPushedEvent();
                    }

                    if (isReply(parsed)) {
                        this->hasReplied = true;
                        this->proxyResult = parsed;
                    }
                } catch (const nlohmann::detail::parse_error &e) {
                    //printf("Non RFC call: %s\n", first_msg.c_str());
                    //printf("error: %s\n", e.what());
                    WARDuino::instance()->handleInterrupt(readAmount, (uint8_t*) first_msg.c_str());
                }
            }
        }
    }
}

bool ProxySupervisor::send(
    void *buffer, int size) {  // TODO buffer needs to be null terminated
    int n = this->channel->write(static_cast<const char *>(buffer));
    return n == size;
}

nlohmann::basic_json<> ProxySupervisor::readReply(RFC *rfc) {
    while (!this->hasReplied) {
        WARDuino::instance()->debugger->checkDebugMessages(
            rfc->m, &WARDuino::instance()->program_state);
    }
    WARDuino::instance()->debugger->channel->write("read reply: succeeded\n");
    printf("read reply: succeeded\n");
    this->hasReplied = false;
    return this->proxyResult;
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
 * output:  1 byte (interrupt code)  | 4 bytes (fidx) | sizeof(arg_1) arg_1  ...
 sizeof(arg_n) arg_n

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
    auto *hexa = new char[hexa_size + 2];  //+2 for '\n' and '0' termination
    chars_as_hexa(hexa, buffer, serializationSize);
    hexa[hexa_size] = '\n';
    hexa[hexa_size + 1] = '\0';  // TODO remove zero termination and +2 above

    delete[] buffer;
    auto *message = new SerializeData;
    message->size = hexa_size + 1;
    message->raw = hexa;

    auto transfer = get_backward(callee->m, callee->fidx);
    this->channel->write(transfer.c_str());

    return message;
}

void ProxySupervisor::deserializeRFCResult(RFC *rfc) {
    nlohmann::basic_json<> call_result = this->readReply(rfc);  // blocking
    rfc->success = *call_result.find("success");

    if (rfc->type->result_count == 0) {
        return;
    }

    //    if (!rfc->success) {
    //        uint16_t msg_size = 0;
    //        memcpy(&msg_size, call_result + 1, sizeof(uint16_t));
    //        if (msg_size > rfc->exception_size) {
    //            delete[] rfc->exception;
    //            rfc->exception = new char[msg_size];
    //            rfc->exception_size = msg_size;
    //        }
    //        memcpy(rfc->exception, call_result + 1 + sizeof(uint16_t),
    //        msg_size); return;
    //    }

    uint8_t type = *call_result.find("type");
    auto *result = (StackValue *)malloc(sizeof(struct StackValue));
    switch (type) {
        case I32:
            result->value.int32 = {*call_result.find("value")};
            break;
        case I64:
            result->value.int64 = {*call_result.find("value")};
            break;
        case F32:
            result->value.f32 = {*call_result.find("value")};
            break;
        case F64:
            result->value.f64 = {*call_result.find("value")};
            break;
        default:
            FATAL("Invalid proxy value type %" PRIu8 "\n", type);
    }
    result->value_type = type;
    rfc->result = result;
}

bool ProxySupervisor::call(RFC *callee) {
    printf("serializing RFC\n");
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
    //    char cmdBuffer[10] = "";
    //    int cmdBufferLen = 0;
    //    sprintf(cmdBuffer, "%x\n%n", interruptDUMPCallbackmapping,
    //    &cmdBufferLen); this->send(cmdBuffer, cmdBufferLen);
    this->deserializeRFCResult(callee);
    printf("end of supervisor::call(rfc)\n");
    return true;
}

void ProxySupervisor::registerProxiedCall(uint32_t fidx) {
    this->proxied->insert(fidx);
}

void ProxySupervisor::unregisterProxiedCall(uint32_t fidx) {
    this->proxied->erase(fidx);
}

void ProxySupervisor::unregisterAllProxiedCalls() { this->proxied->clear(); }

bool ProxySupervisor::isProxied(uint32_t fidx) {
    return this->proxied->count(fidx) > 0;
}
