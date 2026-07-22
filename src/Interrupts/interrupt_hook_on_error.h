#pragma once

#include "../Instrumentation/hook.h"
#include "../Instrumentation/instrumentation.h"

#define HOOK_ON_ERROR_ERROR_CODE_INVALID_INTERRUPT_NR 1;
#define HOOK_ON_ERROR_ERROR_CODE_UNALLOWED_HOOK 2;

typedef struct HookOnErrorRequest {
    uint32_t id{};
    Hook *hook{};
} HookOnErrorRequest;

typedef struct HookOnErrorResponse {
    uint32_t id{};
    uint8_t type{};
    uint8_t error_code{};
} HookOnErrorResponse;

void Interrupt_HookOnError_handle_request(const Channel &requester,
                                          InstrumentationManager &manager,
                                          DebugMessage *msg);

bool Interrupt_HookOnError_deserialize_request(HookOnErrorRequest &dest,
                                               DebugMessage *msg,
                                               uint8_t &error_code);

ssize_t Interrupt_HookOnError_serialize_response(
    const HookOnErrorResponse &response, char *dest);

void Interrupt_HookOnError_send_response(const Channel &channel,
                                         const HookOnErrorResponse &response);

void Interrupt_HookOnError_send_JSON_subscription(const Channel &output,
                                                  uint32_t id, bool start);
