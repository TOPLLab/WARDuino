#pragma once
#include "../Instrumentation/instrumentation.h"

#define AROUND_FUNC_ERROR_CODE_NO_ERROR_CODE_SET 0
#define AROUND_FUNC_ERROR_CODE_UNEXISTING_AROUND_KIND 1
#define AROUND_FUNC_ERROR_CODE_UNEXISTING_LOCAL_FUNC 2
#define AROUND_FUNC_ERROR_CODE_NO_MEMORY_LEFT 3
#define AROUND_FUNC_ERROR_CODE_AROUND_ALREADY_EXISTS 4
#define AROUND_FUNC_ERROR_CODE_SCHEDULING_MODE_NOT_SUPPORTED 5
#define AROUND_FUNC_ERROR_CODE_SUBSTITUE_VALUE_IS_MALFORMED 6
#define AROUND_FUNC_ERROR_CODE_UNSUPPORTED_HOOK 7
#define AROUND_FUNC_ERROR_CODE_REMOVE_HOOKS_FAILED 8
#define AROUND_FUNC_ERROR_CODE_REQUEST_HAS_WRONG_INTERRUPT_NR 9

typedef struct AroundFunctionResponse {
    uint32_t id{};
    uint8_t error_code{AROUND_FUNC_ERROR_CODE_NO_ERROR_CODE_SET};
    uint8_t type{};
} AroundFunctionResponse;

typedef struct AroundFunctionRequest {
    uint32_t id;
    uint32_t func_idx;
    bool addHook;
    Hook hook;
} AroundFunctionRequest;

void Interrupt_AroundFunction_handle_request(const Channel &channel,
                                             InstrumentationManager &manager,
                                             Module *m, DebugMessage *msg);

void Interrupt_AroundFunction_send_response(
    const Channel &channel, const AroundFunctionResponse &response);

bool Interrupt_AroundFunction_deserialize_request(AroundFunctionRequest &dest,
                                                  DebugMessage *msg,
                                                  uint8_t &error_code);
