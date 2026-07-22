#pragma once
#include <sys/types.h>

#include <cstdint>

#include "../Debug/debug_msg.h"
#include "../Utils/sockets.h"
#include "../WARDuino/structs.h"

#define INSPECT_ERROR_CODE_REQUEST_HAS_INVALID_INTERRUPT_NR 1
#define INSPECT_ERROR_CODE_REQUEST_HAS_INVALID_STATE_KIND 2

enum ExecutionState {
    pcState = 0x01,
    breakpointsState = 0x02,
    callstackState = 0x03,
    globalsState = 0x04,
    tableState = 0x05,
    memoryState = 0x06,
    branchingTableState = 0x07,
    stackState = 0x08,
    callbacksState = 0x09,
    eventsState = 0x0A,
    errorState = 0x0B,
    logicalClock = 0x0C,
    heapState = 0x0D,
};

typedef struct InspectStateRequest {
    uint16_t numberOfInspects{};
    ExecutionState *requestedState{};
} InspectStateRequest;

typedef InspectStateRequest StateToInspect;

void Interrupt_Inspect_handle_request(const Channel &requester, Module *m,
                                      DebugMessage *msg);

bool Interrupt_Inspect_deserialize_request(InspectStateRequest &request,
                                           uint8_t *data, uint8_t &error_code);

bool Interrupt_Inspect_inspect_json_output(const Channel &requester,
                                           const Module *m,
                                           const StateToInspect &state,
                                           bool includeHeader = true,
                                           bool includeNewline = true);

bool Interrupt_Inspect_copy_state_to_inspect(
    StateToInspect &dest, const StateToInspect &state_to_cpy);

StateToInspect *Interrupt_Inspect_new_state_to_inspect();

void Interrupt_Inspect_free_state_to_inspect(StateToInspect *to_free);