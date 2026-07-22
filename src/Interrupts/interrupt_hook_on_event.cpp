#include "interrupt_hook_on_event.h"

#include <cstdint>

#include "../Interrupts/interrupt_response.h"
#include "../Utils/util.h"
#include "interrupts.h"

#define RESPONSE_BUFFER_SIZE 10

bool addHook(InstrumentationManager &manager, Hook &hook,
             HookEventMoment moment, uint8_t &error_code);

void Interrupt_HookOnEvent_handle_request(const Channel &requester,
                                          InstrumentationManager &manager,
                                          DebugMessage *msg) {
    OnEventHookRequest request{};
    OnEventHookResponse response{};
    Hook hook;
    request.hook = &hook;

    if (Interrupt_HookOnEvent_deserialize_request(request, msg,
                                                  response.error_code) &&
        addHook(manager, *request.hook, request.moment, response.error_code)) {
        response.type = INTERRUPT_RESPONSE_TYPE_SUCCESS;
    } else {
        response.type = INTERRUPT_RESPONSE_TYPE_ERROR;
    }

    response.id = msg->id;
    Interrupt_HookOnEvent_send_response(requester, response);
}

bool Interrupt_HookOnEvent_deserialize_request(OnEventHookRequest &dest,
                                               DebugMessage *msg,
                                               uint8_t &error_code) {
    // format: interrupt nr | hook moment | Hook
    if (msg->interrupt != interruptHookOnEvent) {
        error_code = ON_EVENT_HOOK_ERROR_CODE_INVALID_INTERRUPT_NR;
        return false;
    }

    uint8_t *encoded_request = msg->data;
    dest.moment = (HookEventMoment)*encoded_request++;
    switch (dest.moment) {
        case HookOnNewEvent:
        case HookOnEventHandling:
        case HookAfterEventHandled:
            break;
        default:
            error_code = ON_EVENT_HOOK_ERROR_CODE_INVALID_HOOK_MOMENT;
            return false;
    }
    dest.id = msg->id;
    return Hooks_deserialize_hook(*dest.hook, msg->id, &encoded_request,
                                  error_code);
}

void Interrupt_HookOnEvent_send_response(const Channel &output,
                                         const OnEventHookResponse &response) {
    char serialization[RESPONSE_BUFFER_SIZE];
    if (Interrupt_OnEventHook_serialize_response(response, serialization)) {
        output.write("%s\n", serialization);
    }
}

bool Interrupt_OnEventHook_serialize_response(
    const OnEventHookResponse &response, char *dest) {
    return Interrupt_HookOnEvent_serialize_hexa_string_response(response, dest);
}

bool Interrupt_HookOnEvent_serialize_hexa_string_response(
    const OnEventHookResponse &response, char *dest) {
    return Interrupt_serialize_hexa_string_response(
               interruptHookOnEvent, response.id, response.type, dest) > -1;
}

void Interrupt_HookOnEvent_send_JSON_subscription(const Channel &output,
                                                  uint32_t id,
                                                  HookEventMoment moment,
                                                  bool start) {
    if (start) {
        bool hasSubContent = true;
        Interrupt_send_JSON_start_message(output, interruptHookOnEvent,
                                          INTERRUPT_RESPONSE_TYPE_SUBSCRIPTION,
                                          id, hasSubContent, NO_ERROR);
        output.write(R"({"moment":"%02X","val":)", moment);
        return;
    }
    output.write("}");
    Interrupt_send_JSON_end_message(output);
}

void Interrupt_HookOnEvent_send_Binary_subscribe_message(const Channel &output,
                                                         const uint32_t id,
                                                         const Event &ev) {
    // format: interrupt_nr (1byte) | message_type (1byte) | id | event
    // id: LEB encoding
    // event: topic | payload
    // topic: size topic (LEB32) | topic (LEB32)
    // payload: size payload (LEB32) | payload (LEB32)

    // calculate total size buffer:
    size_t encodingSize =
        1 + 1 + size_for_32BIT_LEB(id) + size_for_64BIT_LEB(ev.topic.size()) +
        ev.topic.size() + size_for_64BIT_LEB(ev.payload.size()) +
        ev.payload.size();
    uint8_t *buffer = (uint8_t *)malloc(encodingSize);
    if (buffer == nullptr) {
        return;
    }

    // write interrupt nr
    buffer[0] = interruptHookOnEvent;
    buffer[1] = INTERRUPT_RESPONSE_TYPE_SUBSCRIPTION;

    // TODO refactor the following event encoding
    // write Topic size and content
    size_t offset = 2;
    offset += write_32BIT_LEB(id, buffer + offset);
    offset += write_64BIT_LEB(ev.topic.size(), buffer + offset);
    std::memcpy(buffer + offset, ev.topic.c_str(), ev.topic.size());
    offset += ev.topic.size();

    // write payload size and content
    offset += write_64BIT_LEB(ev.payload.size(), buffer + offset);
    std::memcpy(buffer + offset, ev.payload.c_str(), ev.payload.size());

    HexUInt8Encoding dest{};
    if (uint8_to_hex(buffer, encodingSize, &dest)) {
        output.write("%s\n", dest.encoding);
        free(dest.encoding);
    }
    free(buffer);
}

bool addHook(InstrumentationManager &manager, Hook &hook,
             HookEventMoment moment, uint8_t &error_code) {
    bool success = true;
    switch (moment) {
        case HookOnNewEvent:
            if (!manager.addHookOnNewEvent(hook)) {
                error_code = HOOK_ON_EVENT_ERROR_CODE_UNALLOWED_HOOK;
                success = false;
            }
            break;
        case HookOnEventHandling:
            if (!manager.addHookOnEventHandling(hook)) {
                error_code = HOOK_ON_EVENT_ERROR_CODE_UNALLOWED_HOOK;
                success = false;
            }
        default:
            break;
    }
    return success;
}
