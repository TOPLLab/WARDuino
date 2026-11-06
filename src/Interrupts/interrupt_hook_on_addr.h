#pragma once
#include <stddef.h>

#include "../Instrumentation/hook.h"
#include "../Instrumentation/instrumentation.h"
#include "../WARDuino/structs.h"

#define HOOK_ON_ADDR_ERROR_CODE_REQUEST_HAS_WRONG_INTERRUPT_NR 1
#define HOOK_ON_ADDR_ERROR_CODE_REQUEST_HAS_UN_EXISTING_MOMENT 2
#define HOOK_ON_ADDR_ERROR_CODE_REQUEST_HAS_UNEXISTING_ADDR 3;
#define HOOK_ON_ADDR_ERROR_CODE_COULD_NOT_ADD_HOOK 4;
#define HOOK_ON_ADDR_ERROR_CODE_COULD_NOT_REMOVE_HOOK 5;

typedef struct HookOnAddrRequest {
    uint32_t id{};
    uint32_t addr{};
    bool add{};  // true add hook, if false remove hooks
    HookMoment moment{};
    Hook *hook{};
} HookOnAddrRequest;

#define HOOK_AROUND_RESULT_ERROR_CODE_REQUEST_HAS_WRONG_INTERRUPT_NR 1
#define HOOK_AROUND_RESULT_ERROR_CODE_INVALID_KIND 2
#define HOOK_AROUND_RESULT_ERROR_CODE_INVALID_VALUE 3
#define HOOK_AROUND_RESULT_ERROR_CODE_VM_NOT_PAUSED 4
#define HOOK_AROUND_RESULT_ERROR_CODE_NO_AROUND_HOOK 5
#define HOOK_AROUND_RESULT_ERROR_CODE_PC_NOT_AT_ADDR 6
#define HOOK_AROUND_RESULT_ERROR_CODE_NOT_AT_FUNCTION_START 7
#define HOOK_AROUND_RESULT_ERROR_CODE_UNSUPPORTED_INSTRUCTION 8
#define HOOK_AROUND_RESULT_ERROR_CODE_STACK_UNDERFLOW 9
#define HOOK_AROUND_RESULT_ERROR_CODE_RESULT_MISMATCH 10
#define HOOK_AROUND_RESULT_ERROR_CODE_STACK_OVERFLOW 11

typedef struct HookOnAddrResponse {
    uint8_t type{};
    uint8_t error_code{};
    uint32_t id{};
} HookOnAddrResponse;

void Interrupt_HookOnAddr_handle_request(const Channel &channel, Module &module,
                                         InstrumentationManager &manager,
                                         DebugMessage *msg);

bool Interrupt_HookOnAddr_deserialize_request(HookOnAddrRequest &dest,
                                              DebugMessage *msg,
                                              uint8_t &error_code);

void Interrupt_HookOnAddr_send_response(const Channel &channel,
                                        const HookOnAddrResponse &response);

void Interrupt_HookOnAddr_send_subscription(const Channel &output,
                                            HookMoment moment, uint32_t addr,
                                            uint32_t id, bool start);

void Interrupt_HookAroundResult_handle_request(const Channel &channel,
                                               Module &module,
                                               InstrumentationManager &manager,
                                               DebugMessage *msg);

bool Interrupt_HookAroundResult_deserialize_request(
    HookAroundResultRequest &dest, DebugMessage *msg, uint8_t &error_code);
