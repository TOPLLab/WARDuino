#include "interrupt_around_function.h"

#include <cstdint>

#include "../Interrupts/interrupt_response.h"
#include "../Utils/util.h"
#include "interrupts.h"

/*
 * Declaration private functions
 */
bool registerOrUnregisterAroundFunctionHook(
    InstrumentationManager &manager, Module &m,
    const AroundFunctionRequest &request, uint8_t &error_code);

/*
 * Public functions
 */

void Interrupt_AroundFunction_send_response(
    const Channel &channel, const AroundFunctionResponse &response) {
    char *subContent = nullptr;
    Interrupt_send_JSON_message(channel, interruptAroundFunction, response.type,
                                response.id, subContent, response.error_code);
}

void Interrupt_AroundFunction_handle_request(const Channel &channel,
                                             InstrumentationManager &manager,
                                             Module *m, DebugMessage *msg) {
    AroundFunctionRequest request;
    StackValue val;
    request.hook.value.result = &val;  // trick to avoid malloc

    AroundFunctionResponse response;
    uint8_t error{};

    if (Interrupt_AroundFunction_deserialize_request(request, msg, error) &&
        registerOrUnregisterAroundFunctionHook(manager, *m, request, error)) {
        response.type = INTERRUPT_RESPONSE_TYPE_SUCCESS;
    } else {
        response.type = INTERRUPT_RESPONSE_TYPE_ERROR;
        response.error_code = error;
    }
    response.id = msg->id;
    Interrupt_AroundFunction_send_response(channel, response);
}

bool Interrupt_AroundFunction_deserialize_request(AroundFunctionRequest &dest,
                                                  DebugMessage *msg,
                                                  uint8_t &error_code) {
    // format: interrupt nr (1 byte) | Target func (LEB32)
    // | add or Remove (1 byte) | Schedule | Hook
    if (msg->interrupt != interruptAroundFunction) {
        error_code = AROUND_FUNC_ERROR_CODE_REQUEST_HAS_WRONG_INTERRUPT_NR;
        return false;
    }
    uint8_t *data = msg->data;
    dest.func_idx = read_LEB_32(&data);
    dest.addHook = *data++;
    bool success = true;
    if (dest.addHook) {
        success = Hooks_deserialize_hook(dest.hook, msg->id, &data, error_code);
        if (success) {
            switch (dest.hook.kind) {
                case ValueSubstitution:
                case RemoteCall:
                    break;
                case ProxyCall:
                    dest.hook.value.target_fidx = dest.func_idx;
                    break;
                default:
                    error_code = AROUND_FUNC_ERROR_CODE_UNSUPPORTED_HOOK;
                    success = false;
                    break;
            }
        }
    }
    dest.id = msg->id;
    return success;
}

/*
 * Private functions
 */

bool registerOrUnregisterAroundFunctionHook(
    InstrumentationManager &manager, Module &m,
    const AroundFunctionRequest &request, uint8_t &error_code) {
    if (request.func_idx >= m.function_count) {
        error_code = AROUND_FUNC_ERROR_CODE_UNEXISTING_LOCAL_FUNC;
        return false;
    }

    if (request.addHook) {
        if (!manager.isAddHookAroundFuncAllowed(request.func_idx)) {
            error_code = AROUND_FUNC_ERROR_CODE_AROUND_ALREADY_EXISTS;
            return false;
        }

        if (!manager.addHookAroundFunction(m, request.func_idx, request.hook)) {
            error_code = AROUND_FUNC_ERROR_CODE_NO_MEMORY_LEFT;  // TODO change
                                                                 // error_code
            return false;
        }
        return true;
    }

    if (!manager.removeHooksAroundFunction(m, request.func_idx)) {
        error_code = AROUND_FUNC_ERROR_CODE_REMOVE_HOOKS_FAILED;
        return false;
    }

    return true;
}