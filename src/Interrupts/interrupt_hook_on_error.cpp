#include "interrupt_hook_on_error.h"

#include <cstdint>

#include "../Interrupts/interrupt_response.h"
#include "interrupts.h"

#define NO_ERROR 0  // TODO remove

bool addHook(InstrumentationManager &manager, Hook &hook, uint8_t &error_code);

void Interrupt_HookOnError_handle_request(const Channel &requester,
                                          InstrumentationManager &manager,
                                          DebugMessage *msg) {
    HookOnErrorRequest request{};
    HookOnErrorResponse response{};
    Hook hook;
    request.hook = &hook;

    if (Interrupt_HookOnError_deserialize_request(request, msg,
                                                  response.error_code) &&
        addHook(manager, *request.hook, response.error_code)) {
        response.type = INTERRUPT_RESPONSE_TYPE_SUCCESS;
    } else {
        response.type = INTERRUPT_RESPONSE_TYPE_ERROR;
    }
    request.id = msg->id;

    Interrupt_HookOnError_send_response(requester, response);
}

bool Interrupt_HookOnError_deserialize_request(HookOnErrorRequest &dest,
                                               DebugMessage *msg,
                                               uint8_t &error_code) {
    // format: interrupt nr | Hook
    if (msg->interrupt != interruptHookOnError) {
        error_code = HOOK_ON_ERROR_ERROR_CODE_INVALID_INTERRUPT_NR;
        return false;
    }
    dest.id = msg->id;
    uint8_t *encoded_request = msg->data;
    return Hooks_deserialize_hook(*dest.hook, msg->id, &encoded_request,
                                  error_code);
}

void Interrupt_HookOnError_send_response(const Channel &channel,
                                         const HookOnErrorResponse &response) {
    char serialization[10];
    if (Interrupt_HookOnError_serialize_response(response, serialization) > 0) {
        channel.write("%s\n", serialization);
    }
}

ssize_t Interrupt_HookOnError_serialize_response(
    const HookOnErrorResponse &response, char *dest) {
    return Interrupt_serialize_hexa_string_response(
        interruptHookOnError, response.id, response.type, dest);
}

bool addHook(InstrumentationManager &manager, Hook &hook, uint8_t &error_code) {
    if (!manager.addHookOnError(hook)) {
        error_code = HOOK_ON_ERROR_ERROR_CODE_UNALLOWED_HOOK;
        return false;
    }
    return true;
}

void Interrupt_HookOnError_send_JSON_subscription(const Channel &output,
                                                  uint32_t id, bool start) {
    if (start) {
        bool hasSubContent = true;
        Interrupt_send_JSON_start_message(output, interruptHookOnError,
                                          INTERRUPT_RESPONSE_TYPE_SUBSCRIPTION,
                                          id, hasSubContent, NO_ERROR);
        return;
    }
    Interrupt_send_JSON_end_message(output);
}