#pragma once

#include "../Instrumentation/hook.h"
#include "../Instrumentation/instrumentation.h"
#include "../Utils/sockets.h"
#include "../WARDuino/event_structs.h"
#include "../WARDuino/structs.h"
#include "./interrupt_hook_on_event_struct.h"

#define ON_EVENT_HOOK_ERROR_CODE_INVALID_INTERRUPT_NR 1;
#define ON_EVENT_HOOK_ERROR_CODE_INVALID_HOOK_MOMENT 2;
#define HOOK_ON_EVENT_ERROR_CODE_UNALLOWED_HOOK 3;

class InstrumentationManager;  // Fix cyclic dependency by moving struct in
                               // separate file
typedef struct OnEventHookResponse {
    uint32_t id{};
    uint8_t type{};
    uint8_t error_code{};
} OnEventHookResponse;

void Interrupt_HookOnEvent_handle_request(const Channel &requester,
                                          InstrumentationManager &manager,
                                          DebugMessage *msg);

bool Interrupt_HookOnEvent_deserialize_request(OnEventHookRequest &dest,
                                               DebugMessage *msg,
                                               uint8_t &error_code);

void Interrupt_HookOnEvent_send_response(const Channel &output,
                                         const OnEventHookResponse &response);

bool Interrupt_OnEventHook_serialize_response(
    const OnEventHookResponse &response, char *dest);

bool Interrupt_HookOnEvent_serialize_hexa_string_response(
    const OnEventHookResponse &response, char *dest);

void Interrupt_HookOnEvent_send_JSON_subscription(const Channel &output,
                                                  uint32_t id,
                                                  HookEventMoment moment,
                                                  bool start);

void Interrupt_HookOnEvent_send_Binary_subscribe_message(const Channel &output,
                                                         const uint32_t id,
                                                         const Event &ev);
