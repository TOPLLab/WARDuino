#include "interrupt_remote_call.h"

#include <cstdint>

#include "../Interrupts/interrupt_response.h"
#include "../Utils/macros.h"
#include "../WARDuino/vm_exception.h"

class ChannelReader {
   private:
    Channel &channel;
    uint32_t buffer_size = 0;
    uint32_t buffer_max_size = 100;
    char buffer[100]{};
    std::deque<std::string> lines;

    int readUntilChar(char deliminator) {
        ssize_t readAmount{};
        size_t offset = 0;
        bool found_until_char = false;
        while ((readAmount = this->channel.read(
                    buffer + offset, buffer_max_size - buffer_size)) != -1) {
            // Split buffer into strings via deliminator
            for (int i = buffer_size; i < buffer_size + readAmount; ++i) {
                if (buffer[i] == deliminator) {
                    std::string content(buffer + offset, buffer + i);
                    lines.push_back(content);
                    offset = i + 1;
                    found_until_char = true;
                }
            }

            // move remaining buffer content to start buffer
            int idx = 0;
            while (offset < buffer_size + readAmount) {
                buffer[idx++] = buffer[offset++];
            }
            buffer[idx] = '\0';
            buffer_size = idx;
            if (found_until_char) {
                break;
            }
        }

        if (readAmount == -1) {
            return -1;
        }

        return offset;
    }

   public:
    ChannelReader(Channel &t_channel) : channel(t_channel) {}

    int readUntilChar(std::string &dest, char deliminator) {
        int bytes_read = -1;
        if (this->lines.empty()) {
            bytes_read = this->readUntilChar(deliminator);
        }
        if (this->lines.empty()) {
            return bytes_read;
        }
        dest = this->lines.front();
        this->lines.pop_front();
        return dest.length();
    }

    int readNextHexaLine(std::string &line) {
        int bytesRead{};
        while ((bytesRead = this->readUntilChar(line, '\n')) != -1) {
            bool foundHexaLine = true;
            for (char c : line) {
                if (!is_hexa_char(c)) {
                    foundHexaLine = false;
                    // TODO remove
                    printf("Received a non hexa char %c in line %s\n", c,
                           line.c_str());
                    break;
                }
            }
            if (foundHexaLine) {
                // TODO remove
                printf("Found hexa line %s\n", line.c_str());
                break;
            }
        }
        return bytesRead;
    }

    int readLine(std::string &line) { return this->readUntilChar(line, '\n'); }
};

uint8_t char_to_uint8(char c) {
    switch (c) {
        case '0' ... '9':
            return c - '0';
        case 'A' ... 'F':
            return c - 'A' + 10;
        case 'a' ... 'f':
            return c - 'e' + 10;
    }
    FATAL("Func should not be used with a non char\n");
    return 0;
}

uint8_t *hex_to_uint8_t(char *hexString) {
    size_t hexStringLength = strlen(hexString);

    size_t number_hex_chars = 0;
    for (size_t i = 0; i < hexStringLength; i++) {
        if (!is_hexa_char(hexString[i])) {
            break;
        }
        number_hex_chars++;
    }

    if (number_hex_chars % 2 != 0) {
        return nullptr;
    }

    uint8_t *buff = (uint8_t *)malloc(number_hex_chars / 2);
    for (size_t i = 0; i < number_hex_chars / 2; i++) {
        uint8_t v = (char_to_uint8(hexString[i * 2]) << 4u) +
                    char_to_uint8(hexString[i * 2 + 1]);
        buff[i] = v;
    }
    return buff;
}

/*
 * Suppose VM A and VM B
 *
 * The following functions are used by VM A to send a remote
 * call or proxy call request to VM B
 */

char *Interrupt_RemoteCall_serialize_request(FunCallRequest &request,
                                             size_t &serialized_size) {
    // format: interrupt | LEB32 funcid | Stack | newline | null termination

    const ValueSerializationConfig config{
        .includeIndex = false,
        .includeType = false,
    };

    // serialisation
    size_t total_size =
        1 + size_for_32BIT_LEB(request.fun) +
        size_for_stackvalues(request.args, request.number_args, config) + 2;
    uint8_t *buffer = (uint8_t *)malloc(total_size);

    // add interruptnr
    buffer[0] = request.isProxyCall ? interruptProxyCall : interruptFunCall;
    size_t offset = 1;

    // add LEB128 Func ID
    offset += write_32BIT_LEB(request.fun, buffer + offset);

    // serialize args
    offset += serializeStackValues(request.args, request.number_args, config,
                                   buffer + offset);

    HexUInt8Encoding dest{};
    char *encoding{};
    if (uint8_to_hex(buffer, offset, &dest)) {
        serialized_size = offset * 2;
        encoding = dest.encoding;
    } else {
        serialized_size = 0;
    }
    free(buffer);
    return encoding;
}

bool Interrupt_RemoteCall_deserialize_response(FunCallResponse *response,
                                               uint8_t *hex_encoded_response,
                                               const size_t size_response) {
    // format: error type msg
    // interrupt nr (1byte) | message_type (1 byte) | error_code (1 byte) |
    // newline

    // format: success type msg
    // format header: interrupt nr (1byte) | message_type (1 byte) | success
    // (1 byte)

    // format body 1: has_value (1 byte) | StackValue (optional) OR format
    // body 2: has_excp_msg (1 byte) | excp_msg (optional) end: newline

    // Only two possible types of responses
    uint8_t *encoded_response = hex_to_uint8_t((char *)hex_encoded_response);
    if (encoded_response[0] != interruptFunCall ||
        (encoded_response[1] != INTERRUPT_RESPONSE_TYPE_ERROR &&
         encoded_response[1] != INTERRUPT_RESPONSE_TYPE_SUCCESS)) {
        return false;
    }

    response->type = encoded_response[1];
    if (response->type == INTERRUPT_RESPONSE_TYPE_ERROR) {
        response->error_code = encoded_response[2];
        return true;
    }

    CallResult *result = new CallResult;
    result->success = encoded_response[2];
    result->value = nullptr;
    result->exception_msg = nullptr;
    bool has_val_or_excpmsg = encoded_response[3];
    if (has_val_or_excpmsg) {
        if (result->success) {
            const ValueSerializationConfig config{.includeIndex = false,
                                                  .includeType = true};
            result->value = new StackValue;
            deserializeStackValue(result->value, config, encoded_response + 4);
        } else {
            size_t str_size = size_response - 5;
            char *excp_msg = (char *)malloc(str_size);
            memcpy(excp_msg, encoded_response + 4, str_size);
            result->exception_msg = excp_msg;
        }
    }
    response->type = INTERRUPT_RESPONSE_TYPE_SUCCESS;
    response->result = result;
    return true;
}

bool sendRemoteCallRequest(Channel &channel, FunCallRequest &request,
                           uint8_t &error_code) {
    size_t request_size{};
    char *request_encoded =
        Interrupt_RemoteCall_serialize_request(request, request_size);
    if (request_encoded == nullptr) {
        error_code = REMOTE_CALL_ERROR_CODE_OUT_OF_MEMORY;
        return false;
    }

    int written = channel.write("%s\n", request_encoded);
    if (written == -1 || written != (ssize_t)(request_size + 1)) {
        error_code = REMOTE_CALL_ERROR_CODE_WRITE_TO_CLIENT;
        return false;
    }
    return true;
}

bool waitForReply(Channel &channel, uint8_t &error_code, std::string &dest) {
    //  TODO: waitForReply should 1. timeout and 2. make it possible to wait for
    //  a valid reply
    // TODO: waitForReply right now just waits for a valid hex line. It should
    // wait 1. for a valid hex line and 2. the hex line decodes to a response of
    // proxy call.
    ChannelReader reader{channel};
    int number_bytes_read{};
    while ((number_bytes_read = reader.readNextHexaLine(dest)) != -1) {
        if (number_bytes_read < 2) {
            // two hex chars needed to convert interrupt nr
            WARDuino::instance()->debugger->channel->write(
                "Skipping hex string %s (not right number of chars)\n",
                dest.c_str());
            continue;
        }

        uint8_t interruptNr =
            (char_to_uint8(dest[0]) << 4u) + char_to_uint8(dest[1]);
        if (interruptNr == interruptFunCall ||
            interruptNr == interruptProxyCall) {
            break;
        }
        WARDuino::instance()->debugger->channel->write(
            "Skipping hexastring %s as it does not start with expected "
            "interrupt nr\n",
            dest.c_str());
    }

    if (number_bytes_read == -1) {
        error_code = REMOTE_CALL_ERROR_CODE_READ_FROM_CLIENT;
        return false;
    }

    // TODO remove
    WARDuino::instance()->debugger->channel->write("Received valid reply %s\n",
                                                   dest.c_str());
    return true;
}

void Interrupt_RemoteCall_call(const uint32_t func, StackValue *args,
                               const uint32_t args_size, Channel &channel,
                               FunCallResponse *response, bool isProxyCall) {
    // prepare request
    FunCallRequest request;
    request.fun = func;
    request.number_args = args_size;
    request.args = args;
    request.isProxyCall = isProxyCall;
    uint8_t error_code;
    std::string encoded_response{};
    if (!sendRemoteCallRequest(channel, request, error_code) ||
        !waitForReply(channel, error_code, encoded_response)) {
        response->type = INTERRUPT_RESPONSE_TYPE_ERROR;
        response->error_code = error_code;
        return;
    }

    if (!Interrupt_RemoteCall_deserialize_response(
            response, (uint8_t *)encoded_response.c_str(),
            encoded_response.length())) {
        response->type = INTERRUPT_RESPONSE_TYPE_ERROR;
        response->error_code = REMOTE_CALL_ERROR_CODE_MALFORMED_RESPONSE;
    }
}

/*
 * Suppose VM A and VM B
 * Suppose VM A does a remote call or proxy call to VM B
 *
 * The following functions are used by VM B to handle the request
 */

void sendResponse(const Channel &channel, const FunCallResponse &response) {
    size_t serialize_size;
    char *encoding =
        Interrupt_RemoteCall_serialize_response(response, serialize_size);
    if (encoding != nullptr) {
        channel.write("%s\n", encoding);
        free(encoding);
    }
}

uint8_t *serialize_success_response(const FunCallResponse &response,
                                    size_t &size_encoding) {
    // format = header | body 1 or body 2
    // header: interrupt (1byte) | message_type (1byte) | success (1byte)
    // body 1: has_value (1 byte) | StackValue (optional)
    // body 2: has_excp_msg (1 byte) | excp_msg (optional)

    // TODO encode ID
    ValueSerializationConfig config{};
    config.includeType = true;
    config.includeIndex = false;

    size_encoding = 4;  // 1 for interruptnr, 1 for message_type, 1 for success,
    // 1 for has_value or has_expc_msg
    if (response.result->success && response.result->value != nullptr) {
        size_encoding +=
            serializeStackValueSize(response.result->value, config);
    } else if (!response.result->success &&
               response.result->exception_msg != nullptr) {
        size_encoding += strlen(response.result->exception_msg);
        printf(
            "TODO: handle the possibility where an exception message got "
            "written");
    }

    uint8_t *encoded_response = (uint8_t *)malloc(size_encoding);
    encoded_response[0] = interruptFunCall;
    encoded_response[1] = response.type;
    encoded_response[2] = response.result->success;
    encoded_response[3] = response.result->success
                              ? response.result->value != nullptr
                              : response.result->exception_msg != nullptr;

    if (response.result->value != nullptr) {
        serializeStackValue(*response.result->value, config,
                            encoded_response + 4);
    } else if (response.result->exception_msg != nullptr) {
        size_t str_len = size_encoding - 3;
        memcpy(encoded_response + 3, response.result->exception_msg, str_len);
    }
    return encoded_response;
}

uint8_t *serialize_error_response(const FunCallResponse &response,
                                  size_t &size_response) {
    // format: header | body1 | (optional) body2
    // header: interrupt nr (1byte) | message_type (1byte)
    // body: error_code (1byte) | has_exception_msg (1byte)
    // body2: exception_msg

    size_response = 3;
    uint8_t *encoded_response = (uint8_t *)malloc(size_response);
    encoded_response[0] = interruptFunCall;
    encoded_response[1] = INTERRUPT_RESPONSE_TYPE_ERROR;
    encoded_response[2] = response.error_code;

    printf(
        "TODO: serialize error call response still misses has_exception and "
        "optional exception msg");
    return encoded_response;
}

char *Interrupt_RemoteCall_serialize_response(const FunCallResponse &response,
                                              size_t &size_response) {
    size_t size_encoding{};
    uint8_t *toEncode =
        response.type == INTERRUPT_RESPONSE_TYPE_SUCCESS
            ? serialize_success_response(response, size_encoding)
            : serialize_error_response(response, size_encoding);

    char *hexa_encoding = nullptr;
    if (toEncode != nullptr) {
        HexUInt8Encoding dest{};
        if (uint8_to_hex(toEncode, size_encoding, &dest)) {
            hexa_encoding = dest.encoding;
        }
        size_response = size_encoding * 2;  // size in hexa
        free(toEncode);
    }
    return hexa_encoding;
}

bool Interrupt_RemoteCall_deserialize_request(const Module *m,
                                              FunCallRequest &request,
                                              DebugMessage *msg,
                                              uint8_t &error_code) {
    // format: interrupt nr | LEB32 funcid | LEB32 nr args | StackValues |
    // newline
    uint8_t interruptNr = msg->interrupt;
    if (interruptNr != interruptFunCall && interruptNr != interruptProxyCall) {
        error_code = REMOTE_CALL_ERROR_CODE_MALFORMED_REQUEST_INTERRUPT_NR;
        // TODO remove
        WARDuino::instance()->debugger->channel->write(
            "ProxyCall request has malformed interruptNr received interrupt "
            "%" PRIu8 "\n",
            interruptNr);
        return false;
    }

    request.isProxyCall = interruptNr == interruptProxyCall;
    uint8_t *encoded_request = msg->data;

    request.fun = read_LEB_32(&encoded_request);
    if (request.fun > m->function_count) {
        error_code = REMOTE_CALL_ERROR_CODE_INVALID_FUNCTION;
        return false;
    }

    Type *type = m->functions[request.fun].type;
    if (type->param_count == 0) {
        request.number_args = 0;
        request.args = nullptr;
        return true;
    }

    uint8_t *data = encoded_request;
    request.number_args = read_LEB_32(&data);
    if (request.number_args != type->param_count) {
        error_code = REMOTE_CALL_ERROR_CODE_INVALID_NUMBER_OF_ARGUMENTS;
        // TODO remove
        WARDuino::instance()->debugger->channel->write(
            "ProxyCall request has invalid number of args expected %" PRIu32
            " received #%" PRIu32 "\n",
            type->param_count, request.number_args);
        return false;
    }

    ValueDeserializationConfig config{.includeIndex = false,
                                      .includeType = false};
    request.args = deserializeStackValues(encoded_request, config, type);
    if (request.args == nullptr) {
        error_code = REMOTE_CALL_ERROR_CODE_MALFORMED_FUNCTION_ARGS;
        // TODO remove
        WARDuino::instance()->debugger->channel->write(
            "ProxyCall request has malformed args\n");
        return false;
    }

    // TODO remove: This next check is temporary
    if (request.isProxyCall && request.fun >= m->import_count) {
        printf(
            "TODO:  Proxy Call of non primitive functions is not yet "
            "supported\n");
        error_code = REMOTE_CALL_ERROR_CODE_INVALID_FUNCTION;
        // TODO remove
        WARDuino::instance()->debugger->channel->write(
            "ProxyCall request of non primtive function not yet supported\n");
        return false;
    }

    return true;
}

void Interrupt_RemoteCall_do_local_call(Module *m, const uint32_t fun,
                                        StackValue *args, uint32_t nr_args,
                                        CallResult &result) {
    WARDuino *instance = WARDuino::instance();
    RunningState current = instance->program_state;
    instance->program_state = WARDUINOrun;
    result.success = WARDuino::instance()->invoke(m, fun, nr_args, args);
    if (result.success) {
        Type *type = m->functions[fun].type;
        if (type->result_count > 0) {
            *result.value = m->stack[m->sp--];
        } else {
            // set value to null as there is no wasm value produced
            result.value = nullptr;
        }
    } else {
        result.exception_msg = strdup(VM_Exception_get_exception());
    }
    instance->program_state = current;
}

void Interrupt_RemoteCall_do_proxy_call(Module *m, const uint32_t func,
                                        StackValue *args,
                                        CallResult &callResult) {
    // adding arguments to the stack
    Type *type = m->functions[func].type;
    for (auto i = 0; i < type->param_count; i++) {
        m->stack[++m->sp] = args[i];
    }

    // proxy call to primitive functions can be immediately called
    if (func < m->import_count) {
        callResult.success = ((Primitive)m->functions[func].func_ptr)(m);
        if (callResult.success) {
            if (type->result_count > 0) {
                *callResult.value = m->stack[m->sp--];
            } else {
                // set value to null as there is no wasm value produced
                callResult.value = nullptr;
            }
        } else if (!callResult.success) {
            callResult.exception_msg = strdup(VM_Exception_get_exception());
        }
    }

    return;
}

void Interrupt_RemoteCall_handle_request(const Channel &requester, Module *m,
                                         DebugMessage *msg) {
    FunCallRequest request{};
    FunCallResponse response{};
    StackValue resultValue{};
    CallResult result{};
    uint8_t error_code{};

    result.value = &resultValue;  // trick to avoid malloc
    response.result = &result;

    if (Interrupt_RemoteCall_deserialize_request(m, request, msg, error_code)) {
        if (request.isProxyCall) {
            Interrupt_RemoteCall_do_proxy_call(m, request.fun, request.args,
                                               result);
        } else {
            Interrupt_RemoteCall_do_local_call(m, request.fun, request.args,
                                               request.number_args, result);
        }
        response.type = INTERRUPT_RESPONSE_TYPE_SUCCESS;
    } else {
        response.type = INTERRUPT_RESPONSE_TYPE_ERROR;
        response.error_code = error_code;
    }
    response.id = msg->id;

    sendResponse(requester, response);
}