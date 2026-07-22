#include "interrupt_hook_on_addr.h"

#include <cstdint>

#include "../Utils/util.h"
#include "interrupt_response.h"
#include "interrupts.h"

/*
 * Declaration private functions
 */
bool registerHookOnAddr(InstrumentationManager &manager, Module &m,
                        const HookOnAddrRequest &request, uint8_t &error_code);

/*
 * Public functions
 */

void Interrupt_HookOnAddr_handle_request(const Channel &channel, Module &module,
                                         InstrumentationManager &manager,
                                         DebugMessage *msg) {
    Hook hook{};
    HookOnAddrRequest request;
    request.hook = &hook;

    HookOnAddrResponse response;
    uint8_t error{};

    if (Interrupt_HookOnAddr_deserialize_request(request, msg, error) &&
        registerHookOnAddr(manager, module, request, error)) {
        response.type = INTERRUPT_RESPONSE_TYPE_SUCCESS;
    } else {
        response.type = INTERRUPT_RESPONSE_TYPE_ERROR;
        response.error_code = error;
    }
    response.id = msg->id;
    Interrupt_HookOnAddr_send_response(channel, response);
}

bool Interrupt_HookOnAddr_deserialize_request(HookOnAddrRequest &dest,
                                              DebugMessage *msg,
                                              uint8_t &error_code) {
    // format: InterruptNr (1 byte)| addr (LEB32) | HookMoment (1 byte)
    // | add or remove (1 byte) |hook

    if (msg->interrupt != interruptHookOnAddress) {
        error_code = HOOK_ON_ADDR_ERROR_CODE_REQUEST_HAS_WRONG_INTERRUPT_NR;
        return false;
    }

    uint8_t *data = msg->data;
    dest.addr = read_LEB_32(&data);
    dest.moment = (HookMoment)*data++;
    switch (dest.moment) {
        case HookBefore:
        case HookAfter:
            break;
        default:
            error_code = HOOK_ON_ADDR_ERROR_CODE_REQUEST_HAS_UN_EXISTING_MOMENT;
            return false;
    }

    dest.add = *data++;
    dest.id = msg->id;
    if (dest.add) {
        return Hooks_deserialize_hook(*dest.hook, msg->id, &data, error_code);
    }
    // removing hooks so nothing to deserialize
    dest.hook = nullptr;
    return true;
}

void Interrupt_HookOnAddr_send_response(const Channel &channel,
                                        const HookOnAddrResponse &response) {
    Interrupt_send_JSON_message(channel, interruptHookOnAddress, response.type,
                                response.id, nullptr, response.error_code);
}

/*
 * Private functions
 */

bool registerHookOnAddr(InstrumentationManager &manager, Module &m,
                        const HookOnAddrRequest &request, uint8_t &error_code) {
    if (!isToPhysicalAddrPossible(request.addr, &m)) {
        error_code = HOOK_ON_ADDR_ERROR_CODE_REQUEST_HAS_UNEXISTING_ADDR;
        return false;
    }
    if (request.add) {
        if (!manager.addHookOnWasmAddress(m, request.addr, *request.hook,
                                          request.moment)) {
            error_code = HOOK_ON_ADDR_ERROR_CODE_COULD_NOT_ADD_HOOK;
            return false;
        }
    } else {
        if (!manager.removeHooksOnWasmAddress(m, request.addr,
                                              request.moment)) {
            error_code = HOOK_ON_ADDR_ERROR_CODE_COULD_NOT_REMOVE_HOOK;
            return false;
        }
    }
    return true;
}

void Interrupt_HookOnAddr_send_subscription(const Channel &output,
                                            HookMoment moment, uint32_t addr,
                                            uint32_t id, bool start) {
    if (start) {
        bool hasSubContent = true;
        Interrupt_send_JSON_start_message(output, interruptHookOnAddress,
                                          INTERRUPT_RESPONSE_TYPE_SUBSCRIPTION,
                                          id, hasSubContent, NO_ERROR);
        output.write(R"({"moment":"%02X","addr":"%02X","val":)", moment, addr);
        return;
    }

    output.write("}");  // close sub object
    Interrupt_send_JSON_end_message(output);
}
