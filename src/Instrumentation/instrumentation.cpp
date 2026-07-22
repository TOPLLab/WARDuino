#include "instrumentation.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "../Interrupts/interrupt_hook_on_addr.h"
#include "../Interrupts/interrupt_hook_on_error.h"
#include "../Interrupts/interrupt_hook_on_event.h"
#include "../Interrupts/interrupt_remote_call.h"
#include "../Interrupts/interrupt_response.h"
#include "../Utils/macros.h"
#include "../WARDuino/vm_exception.h"
#include "hook.h"
#include "instrumentation_structs.h"
#include "logical_clock.h"
#include "schedule.h"

InstrumentationManager::InstrumentationManager() {}

void InstrumentationManager::registerAroundFunctionChannel(Channel *channel) {
    this->fun_call_channel = channel;
    this->lastObservedTime.nr_of_events = 0;
    this->lastObservedTime.nr_of_instructions =
        30;  // not set to 0 so the first instr can also be hooked upon
}

HooksPrimitiveFunc *InstrumentationManager::new_Primitive_Instrumentation() {
    return new HooksPrimitiveFunc{};
}

void InstrumentationManager::delete_Primitive_Instrumentation(
    HooksPrimitiveFunc *func) {
    Hooks_free_hooks(func->hook);
    func->original_func = nullptr;
    delete func;
}

HooksWasmAddr *InstrumentationManager::new_WasmAddress_Instrumentation() {
    return new HooksWasmAddr{};
}

bool InstrumentationManager::has_HooksOnAroundFunction(uint32_t funID) {
    return this->hooks_around_prim_funcs.count(funID) > 0;
}

bool InstrumentationManager::has_HookOnWasmAddr(uint32_t addr,
                                                HookMoment moment) {
    switch (moment) {
        case HookBefore:
            return this->instr_wasm_addr_before.count(addr) > 0;
        case HookAfter:
            return this->instr_wasm_addr_after.count(addr) > 0;
        default:
            FATAL("Around monitor not supported");
    }
    return false;
}

bool InstrumentationManager::isAddHookOnEventAllowed(Hook &hook) {
    switch (hook.kind) {
        case EventInspect:
        case EventRemove:
        case StateInspect:
            return true;
        default:
            return false;
    }
}

bool InstrumentationManager::isAddHookOnEventHandlingAllowed(Hook &hook) {
    return this->isAddHookOnEventAllowed(hook);
}

bool InstrumentationManager::isAddHookAroundFuncAllowed(uint32_t funID) {
    if (!this->has_HooksOnAroundFunction(funID)) return true;
    // Dissallows hooks that have been scheduled for always if one is
    // alread in place

    printf(
        "TODO: addHookAroundFunction yet to decide what to do in case of "
        "multiple hooks");

    // TODO: decide whether even if you have always adding this is allowed
    // because it just replaces it or becomes another hook. if the hook kind
    // is always tou replace the old one with the new one if the hook is
    // something else than always such as once then that hook is put in front
    // of the always so that once the once is consumed the always remains.
    Hook *hook = this->hooks_around_prim_funcs[funID]->hook;
    return hook == nullptr || hook->schedule.kind != ScheduleAlways;
}

bool InstrumentationManager::isAddHookOnErrorAllowed(Hook &hook) {
    return hook.kind == StateInspect;
}

bool InstrumentationManager::addHookAroundFunction(Module &m, uint32_t func_idx,
                                                   const Hook &around) {
    if (func_idx > m.function_count) {
        return false;
    } else if (func_idx < m.import_count) {
        HooksPrimitiveFunc *instr =
            this->start_primitive_call_interception(m, func_idx);
        Hook *cpy{};
        if (instr == nullptr || (cpy = Hooks_copyHook(around)) == nullptr) {
            return false;
        }
        instr->hook = Hooks_add_and_sort(instr->hook, cpy);
        return true;
    } else {
        printf("TODO: addHookAroundFunction for non primitive functions\n");
        return false;
    }
}

bool InstrumentationManager::removeHooksAroundFunction(Module &m,
                                                       uint32_t func_idx) {
    return this->stop_primitive_call_interception(m, func_idx);
}

bool InstrumentationManager::removeHooksOnWasmAddress(Module &module,
                                                      uint32_t addr,
                                                      const HookMoment moment) {
    if (!isToPhysicalAddrPossible(addr, &module)) {
        // address is not in module
        return false;
    }
    if (!this->has_HookOnWasmAddr(addr, moment)) {
        return false;
    }

    HooksWasmAddr *instr;
    std::unordered_map<uint32_t, HooksWasmAddr *> *mapForErase;
    bool restoreOpcode;
    if (moment == HookBefore) {
        instr = instr_wasm_addr_before[addr];
        mapForErase = &this->instr_wasm_addr_before;
        restoreOpcode = !this->has_HookOnWasmAddr(addr, HookAfter);
    } else {
        instr = instr_wasm_addr_after[addr];
        mapForErase = &this->instr_wasm_addr_after;
        restoreOpcode = !this->has_HookOnWasmAddr(addr, HookBefore);
    }

    while (instr->hook != nullptr) {
        Hook *hookToFree = instr->hook;
        instr->hook = instr->hook->nextHook;
        Hooks_free_hook(hookToFree);
    }

    if (restoreOpcode) {
        module.bytes[addr] = instr->original_opcode;
    }

    mapForErase->erase(addr);
    delete instr;
    return true;
}

bool InstrumentationManager::addHookOnWasmAddress(Module &module, uint32_t addr,
                                                  Hook &hook,
                                                  const HookMoment moment) {
    if (!isToPhysicalAddrPossible(addr, &module)) {
        // address is not in module
        return false;
    }
    HooksWasmAddr *instr =
        this->start_wasm_addr_intercept(module, addr, moment);

    Hook *cpy{};
    if (instr == nullptr || (cpy = Hooks_copyHook(hook)) == nullptr) {
        return false;
    }
    instr->hook = Hooks_add_and_sort(instr->hook, cpy);
    return true;
}

bool InstrumentationManager::addHookOnNewEvent(Hook &hook) {
    if (!this->isAddHookOnEventAllowed(hook)) {
        return false;
    }
    CallbackHandler::pendingEventsActivated = true;
    Hook *h = new Hook();
    *h = hook;
    h->nextHook = nullptr;
    this->hooksForOnNewEvent = Hooks_add_and_sort(this->hooksForOnNewEvent, h);

    return true;
}

bool InstrumentationManager::addHookOnEventHandling(Hook &hook) {
    if (!this->isAddHookOnEventHandlingAllowed(hook)) {
        return false;
    }

    Hook *h = new Hook();
    *h = hook;
    h->nextHook = nullptr;
    this->hooksForOnEventHandling =
        Hooks_add_and_sort(this->hooksForOnEventHandling, h);
    this->interceptEvents = true;

    return true;
}

bool InstrumentationManager::addHookOnError(Hook &hook) {
    if (!this->isAddHookOnErrorAllowed(hook)) {
        return false;
    }

    Hook *h = new Hook();
    *h = hook;
    h->nextHook = nullptr;
    this->hooksForOnError = Hooks_add_and_sort(this->hooksForOnError, h);
    this->interceptError = true;

    return true;
}

void Interrupt_RemoteCall_free_response(FunCallResponse &response) {
    if (response.result != nullptr) {
        if (response.result->value != nullptr) {
            delete response.result->value;
        }
        if (response.result->exception_msg != nullptr) {
            delete response.result->exception_msg;
        }
    }
}

bool InstrumentationManager::do_remote_call(Channel &channel, Module *module,
                                            uint32_t local_fidx,
                                            uint32_t func_to_call,
                                            bool isProxyCall) {
    if (this->fun_call_channel == nullptr) {
        VM_Exception_write("No channel set to perform around function call");
        return false;
    }

    // get args
    Type *func_type = module->functions[local_fidx].type;
    StackValue *args = nullptr;
    if (func_type->param_count > 0) {
        module->sp -= func_type->param_count;  // pop args
        args = module->stack + module->sp + 1;
    }

    FunCallResponse response;
    Interrupt_RemoteCall_call(func_to_call, args, func_type->param_count,
                              channel, &response, isProxyCall);
    if (response.type == INTERRUPT_RESPONSE_TYPE_ERROR) {
        printf("TODO copy error properly\n");

        printf("Remotecall failed error_code%" PRIu8 " \n",
               response.error_code);
        VM_Exception_write("Remotecall failed error_code%" PRIu8 "",
                           response.error_code);
        return false;
    }

    printf(
        "TODO: check if local fun expects a result and then on "
        "stack\n");

    // Push result on stack
    CallResult *result = response.result;
    if (result->value != nullptr) {
        module->sp += 1;
        StackValue *value = &module->stack[module->sp];
        value->value_type = result->value->value_type;
        value->value = result->value->value;
    } else if (result->exception_msg != nullptr) {
        VM_Exception_write("%s", result->exception_msg);
    }
    bool success = result->success;
    Interrupt_RemoteCall_free_response(response);
    return success;
}

bool InstrumentationManager::do_value_substitution(Module *module,
                                                   uint32_t func_called,
                                                   Hook *hook) {
    Type *type = module->functions[func_called].type;
    module->sp -= type->param_count;  // pop args
    if (type->result_count > 0) {
        if (hook->value.result == nullptr) {
            VM_Exception_write("No Substitute value provided");
            return false;
        }
        module->sp += 1;
        StackValue *value = &module->stack[module->sp];
        value->value_type = hook->value.result->value_type;
        value->value = hook->value.result->value;
    }
    return true;
}

bool InstrumentationManager::runHooksOnInterceptedFuncCall(
    const Channel &output, Module *module, LogicalClock *currentTime,
    RunningState &runningState) {
    uint32_t primitive_called = module->last_called;

    // Before call instrumentation

    // Around call instrumentation(s)

    auto iterator = hooks_around_prim_funcs.find(primitive_called);
    if (iterator == hooks_around_prim_funcs.end() ||
        iterator->second->hook == nullptr) {
        VM_Exception_write(
            "No Instrumentation registered for primitive %" PRIu32 "",
            primitive_called);
        // TODO ADD subscription message for no instrumentation registered
        return false;
    }

    HooksPrimitiveFunc *instr = iterator->second;
    Hook *hookToRun = instr->hook;
    HookArgs args = {.currentTime = *currentTime,
                     .addr = 0,
                     .runningState = runningState,
                     .local_fidx = primitive_called,
                     .hookOnAddressSubContent = nullptr,
                     .hookOnErrorSubContent = nullptr,
                     .hookOnEventSubContent = nullptr,
                     .moment = HookAround,
                     .eventMoment = HookOnNewEvent,
                     .ev = nullptr};
    HooksResult result{};
    this->run_hooks(output, *module, hookToRun, args, result);
    instr->hook = result.hooks_left;
    if (result.one_delayed) {
        // We did not find a hook to run
        // normally we should wait the hook to be scheduled for run
        // by letting the original intercepted function be called
        // for now we pause the execution
        printf(
            "TODO: restore the PC to the position before the call "
            "(and pause the VM)?");
        runningState = WARDUINOpause;
        // TODO send messages via subscription for waiting for incoming
        // event to occur return true;
    } else {
        VM_Exception_write("No hook scheduled for primitive call");
        return false;
    }

    //
    // After call instrumentation(s)
    return result.success;
}

void writeSubscriptionContent(const Channel &output, HookArgs &args, Hook &hook,
                              bool start) {
    if (args.hookOnAddressSubContent != nullptr) {
        args.hookOnAddressSubContent(output, args.moment, args.addr, hook.id,
                                     start);
    } else if (args.hookOnEventSubContent != nullptr) {
        args.hookOnEventSubContent(output, hook.id, args.eventMoment, start);
    } else if (args.hookOnErrorSubContent != nullptr) {
        args.hookOnErrorSubContent(output, hook.id, start);
    } else {
        FATAL("Forgot to register subcontent writer for start subscription\n");
    }
}

HookRunResult InstrumentationManager::run_hook(const Channel &output,
                                               Module &module, Hook &hook,
                                               HookArgs &args) {
    // TODO throw error if a hook is encountered that cannot be scheduled
    // TODO check if pauzing, running still works
    bool do_run = Hook_isScheduledForNow(args.currentTime, hook.schedule);
    if (!do_run) {
        return HookDelayed;
    }
    bool hook_success = false;
    switch (hook.kind) {
        case ProxyCall:
        case RemoteCall:
            hook_success = this->do_remote_call(
                *this->fun_call_channel, &module, args.local_fidx,
                hook.value.target_fidx, hook.kind == ProxyCall);
            break;
        case ValueSubstitution:
            hook_success =
                this->do_value_substitution(&module, args.local_fidx, &hook);
            break;
        case StateInspect: {
            const Module *m = &module;
            writeSubscriptionContent(output, args, hook, true);
            bool includeHeader = false;
            bool includeNewline = false;
            bool success = Interrupt_Inspect_inspect_json_output(
                output, m, *hook.value.state, includeHeader, includeNewline);
            writeSubscriptionContent(output, args, hook, false);
            if (!success) {
                VM_Exception_write("Unexisting State to inspect");
            }
            hook_success = success;
            break;
        }
        case ChangeRunningState:
            args.runningState = hook.value.runState;
            hook_success = true;
            break;
        case EventAdd: {
            CallbackHandler::push_event(hook.value.ev->topic,
                                        hook.value.ev->payload,
                                        hook.value.ev->payload_length);
            hook_success = true;
            break;
        }
        case EventRemove:
            if (args.eventMoment == HookOnNewEvent) {
                if (!CallbackHandler::pendingEvents->empty()) {
                    CallbackHandler::pendingEvents->pop_front();
                }
                hook_success = true;
            } else if (args.eventMoment == HookOnEventHandling) {
                if (!CallbackHandler::events->empty()) {
                    CallbackHandler::events->pop_front();
                }
                hook_success = true;
            } else {
                VM_Exception_write("Unsupported event hook moment");
                hook_success = false;
            }
            break;
        case EventInspect:
            Interrupt_HookOnEvent_send_Binary_subscribe_message(output, hook.id,
                                                                *(args.ev));
            hook_success = true;
            break;
        default:
            VM_Exception_write("About to run an unsupported hook kind");
            hook_success = false;
            break;
    }
    return hook_success ? HookCompleted : HookFailed;
}

void InstrumentationManager::runHooksOnError(const Channel &output,
                                             Module *module,
                                             LogicalClock *currentTime) {
    if (this->hooksForOnError == nullptr) {
        this->stopRunningHooksOnError();
        return;
    }

    RunningState unusedState = WARDUINOpause;
    HookMoment unusedMoment = HookBefore;
    HookEventMoment unusedEventMoment = HookOnNewEvent;
    Hook *hooks = this->hooksForOnError;
    HookArgs args = {
        .currentTime = *currentTime,
        .addr = 0,
        .runningState = unusedState,
        .local_fidx = 0,
        .hookOnAddressSubContent = nullptr,
        .hookOnErrorSubContent = &Interrupt_HookOnError_send_JSON_subscription,
        .hookOnEventSubContent = nullptr,
        .moment = unusedMoment,
        .eventMoment = unusedEventMoment,
        .ev = nullptr};
    HooksResult result{};
    this->run_hooks(output, *module, hooks, args, result);
    this->hooksForOnError = result.hooks_left;
}

bool InstrumentationManager::runHooksAfterWasmAddr(const Channel &output,
                                                   Module *module,
                                                   RunningState &runningState) {
    // Only called when tool client wants to do something after some wasm
    // addr. Inefficiently called after each instruction execution.
    // Benchmark needed to determine whether an alternative approach is
    // required

    while (!this->frames_to_monitor.empty()) {
        MonitoredFrame frame = this->frames_to_monitor.top();
        if (frame.frame_idx < module->csp) {
            break;
        }

        this->frames_to_monitor.pop();

        uint32_t addr = frame.addr;
        if (!has_HookOnWasmAddr(addr, HookAfter)) {
            continue;
        }
        HooksWasmAddr *instr = this->instr_wasm_addr_after[addr];

        auto lc = module->warduino->logicalClock;
        Hook *hooks = instr->hook;
        HookEventMoment unusedEventMoment = HookOnNewEvent;
        HookArgs args = {
            .currentTime = lc,
            .addr = addr,
            .runningState = runningState,
            .local_fidx = 0,
            .hookOnAddressSubContent = &Interrupt_HookOnAddr_send_subscription,
            .hookOnErrorSubContent = nullptr,
            .hookOnEventSubContent = nullptr,
            .moment = HookAfter,
            .eventMoment = unusedEventMoment,
            .ev = nullptr,
        };
        HooksResult res;
        this->run_hooks(output, *module, hooks, args, res);
        instr->hook = res.hooks_left;
        if (!res.success) {
            if (!VM_Exception_has_exception()) {
                VM_Exception_write("Failed to run hook %" PRIu8
                                   " after addr %" PRIu32 "\n",
                                   res.hook_idx, addr);
            }
            return false;
        }

        if (instr->hook == nullptr) {
            // all the hooks got run so no need to instrument on addr
            bool s = this->removeHooksOnWasmAddress(*module, addr, HookAfter);
            if (!s) {
                VM_Exception_write(
                    "failed to remove after hooks on addr %" PRIu32 "\n", addr);
                return false;
            }
        }
    }

    if (this->frames_to_monitor.empty()) {
        this->awakeOnNextInstruction = false;
    }
    return true;
}

bool InstrumentationManager::runHooksOnWasmAddr(const Channel &output,
                                                Module *module,
                                                LogicalClock *currentTime,
                                                uint8_t &opcode,
                                                RunningState &runningState) {
    module->pc_ptr -= 1;  // set pc to start of instruction
    uint32_t addr = toVirtualAddress(module->pc_ptr, module);
    bool success = true;
    bool upcodeRestored = false;

    if (LogicalClock_is_t1_equal_t2(this->lastObservedTime, *currentTime)) {
        // Reentering an addr for which the hooks were just run
        // do not run the hooks but advance computation
        auto instr = this->has_HookOnWasmAddr(addr, HookBefore)
                         ? this->instr_wasm_addr_before[addr]
                         : this->instr_wasm_addr_after[addr];
        opcode = instr->original_opcode;
        module->pc_ptr += 1;
    } else {
        this->lastObservedTime = *currentTime;
        if (this->has_HookOnWasmAddr(addr, HookBefore)) {
            success = this->do_before_wasm_addr_hooks(
                output, *module, *currentTime, addr, opcode, runningState);
            upcodeRestored = true;
        }

        if (runningState != WARDUINOpause) {
            module->pc_ptr += 1;
        }

        if (this->has_HookOnWasmAddr(addr, HookAfter)) {
            // save frame and addr for after addr hooks
            MonitoredFrame frame{};
            frame.addr = addr;
            frame.frame_idx = module->csp;
            this->frames_to_monitor.push(frame);
            this->awakeOnNextInstruction = true;
            if (!upcodeRestored) {
                // When there is no before the opcode needs to be set
                auto instr = this->instr_wasm_addr_after[addr];
                opcode = instr->original_opcode;
            }
        }
    }
    return success;
}

void InstrumentationManager::run_hooks(const Channel &output, Module &module,
                                       Hook *hooks, HookArgs &args,
                                       HooksResult &result) {
    HookRunResult run_result = HookCompleted;
    Hook *next_hook = hooks;
    result.hook_idx = 0;
    while (next_hook != nullptr) {
        run_result = this->run_hook(output, module, *next_hook, args);
        if (run_result == HookDelayed)
            result.one_delayed = true;
        else if (run_result == HookFailed)
            break;
        next_hook = next_hook->nextHook;
        result.hook_idx += 1;
    }
    result.success = run_result != HookFailed;
    if (result.success) {
        result.hooks_left = Hooks_remove_completed_hook(hooks);
    }
}

bool InstrumentationManager::do_before_wasm_addr_hooks(
    const Channel &output, Module &module, LogicalClock &currentTime,
    uint32_t addr, uint8_t &opcode, RunningState &runningState) {
    if (!has_HookOnWasmAddr(addr, HookBefore)) {
        VM_Exception_write(
            "No hook registered on instrumented addr %" PRIu32 "", addr);
        return false;
    }

    HooksWasmAddr *instr = this->instr_wasm_addr_before[addr];
    HookEventMoment unusedMoment = HookOnNewEvent;
    HookArgs args = {
        .currentTime = currentTime,
        .addr = addr,
        .runningState = runningState,
        .local_fidx = 0,
        .hookOnAddressSubContent = &Interrupt_HookOnAddr_send_subscription,
        .hookOnErrorSubContent = nullptr,
        .hookOnEventSubContent = nullptr,
        .moment = HookBefore,
        .eventMoment = unusedMoment,
        .ev = nullptr};
    HooksResult result{};
    this->run_hooks(output, module, instr->hook, args, result);
    if (result.success) opcode = instr->original_opcode;
    return result.success;
}

HooksPrimitiveFunc *InstrumentationManager::start_primitive_call_interception(
    Module &m, uint32_t target_func) {
    if (this->has_HooksOnAroundFunction(target_func)) {
        return this->hooks_around_prim_funcs[target_func];
    }

    // The first time for which an instrumentation occurs for the primitive
    // func
    HooksPrimitiveFunc *instr = this->new_Primitive_Instrumentation();
    if (instr != nullptr) {
        instr->original_func = (Primitive)m.functions[target_func].func_ptr;
        this->hooks_around_prim_funcs[target_func] = instr;
        m.functions[target_func].func_ptr =
            (void (*)()) & Instrumentation_interceptPrimitiveCall;
    }
    return instr;
}

bool InstrumentationManager::stop_primitive_call_interception(
    Module &m, uint32_t target_func) {
    if (!this->has_HooksOnAroundFunction(target_func)) {
        return false;
    }

    HooksPrimitiveFunc *hooks_func = this->hooks_around_prim_funcs[target_func];
    m.functions[target_func].func_ptr =
        (void (*)()) & hooks_func->original_func;

    this->delete_Primitive_Instrumentation(hooks_func);
    this->hooks_around_prim_funcs.erase(target_func);
    return true;
}

uint8_t InstrumentationManager::getOriginalOpcode(Module &module,
                                                  uint32_t addr) {
    if (this->has_HookOnWasmAddr(addr, HookBefore)) {
        return this->instr_wasm_addr_before[addr]->original_opcode;
    } else if (this->has_HookOnWasmAddr(addr, HookAfter)) {
        return this->instr_wasm_addr_after[addr]->original_opcode;
    }
    return module.bytes[addr];
}

HooksWasmAddr *InstrumentationManager::start_wasm_addr_intercept(
    Module &module, const uint32_t addr, HookMoment moment) {
    if (this->has_HookOnWasmAddr(addr, moment)) {
        if (moment == HookBefore) {
            return this->instr_wasm_addr_before[addr];
        } else {
            return this->instr_wasm_addr_after[addr];
        }
    }

    // The first time for which an instrumentation occurs for the wasm
    // address
    uint8_t original = this->getOriginalOpcode(module, addr);
    HooksWasmAddr *instr = this->new_WasmAddress_Instrumentation();
    if (instr != nullptr) {
        instr->address = addr;
        instr->original_opcode = original;
        module.bytes[addr] = INSTRUMENTATION_INTERCEPT_OPCODE;
        instr->hook = nullptr;
        if (moment == HookBefore) {
            this->instr_wasm_addr_before[addr] = instr;
        } else {
            this->instr_wasm_addr_after[addr] = instr;
        }
    }
    return instr;
}

bool InstrumentationManager::runHookForOnEventHandling(const Channel &output,
                                                       Module *module) {
    Hook *hookToRun = this->hooksForOnEventHandling;
    if (hookToRun == nullptr || CallbackHandler::events->empty()) {
        if (hookToRun == nullptr) {
            printf(
                "TODO: hooksForOnEventHandling: there is no hook found to "
                "be "
                "run on newly pushed events. We will now therefore undo "
                "the instrumentation but we still need to decide what "
                "would be the default behaviour. Pause for instance?\n");
            this->stopRunningHooksOnEventsHandled();
        }
        return false;
    }

    WARDuino *wd = WARDuino::instance();
    uint32_t sizeBeforeHooks = CallbackHandler::events->size();
    Event event = CallbackHandler::events->front();
    HookArgs args = {.currentTime = wd->logicalClock,
                     .addr = 0,
                     .runningState = wd->program_state,
                     .local_fidx = 0,

                     .hookOnAddressSubContent = nullptr,
                     .hookOnErrorSubContent = nullptr,
                     .hookOnEventSubContent = nullptr,
                     .moment = HookBefore,
                     .eventMoment = HookOnEventHandling,
                     .ev = &event};
    HooksResult result{};
    this->run_hooks(output, *module, hookToRun, args, result);
    this->hooksForOnEventHandling = result.hooks_left;
    uint32_t sizeAfterHooks = CallbackHandler::events->size();
    bool eventRemoved = args.ev == nullptr;
    if (eventRemoved && sizeAfterHooks >= sizeBeforeHooks) {
        FATAL(
            "TODO: the edge case occurred where new events got pushed into "
            "the "
            "queue (because of interrupts or hooks) while removing ones "
            "because of hooks. You need to introduce IDs for each event to "
            "know when to stop the isntrumentation.\n");
    }
    return eventRemoved && !CallbackHandler::events->empty();
}

void InstrumentationManager::runHooksForOnNewEvent(const Channel &output,
                                                   Module *module) {
    WARDuino *wd = WARDuino::instance();
    HookArgs args = {
        .currentTime = wd->logicalClock,
        .addr = 0,
        .runningState = wd->program_state,
        .local_fidx = 0,
        .hookOnAddressSubContent = nullptr,
        .hookOnErrorSubContent = nullptr,
        .hookOnEventSubContent = &Interrupt_HookOnEvent_send_JSON_subscription,
        .moment = HookBefore,
        .eventMoment = HookOnNewEvent,
        .ev = nullptr};
    HooksResult result{};
    while (!CallbackHandler::pendingEvents->empty()) {
        Hook *hookToRun = this->hooksForOnNewEvent;
        if (hookToRun == nullptr) {
            printf(
                "TODO: runHooksforOnNewEvent: there is no hook found to be "
                "run on newly pushed events. We will now therefore undo "
                "the instrumentation but we still need to decide what "
                "would be the default behaviour. Pause for instance?\n");
            this->stopRunningHooksOnNewEvents();
            return;
        }
        Event *ev = CallbackHandler::pendingEvents->front();
        args.ev = ev;
        this->run_hooks(output, *module, hookToRun, args, result);
        this->hooksForOnNewEvent = result.hooks_left;
        if (!CallbackHandler::pendingEvents->empty() &&
            ev == CallbackHandler::pendingEvents->front()) {
            // event did not get removed by any hook
            // add event to main queue
            CallbackHandler::push_event(ev);
            CallbackHandler::pendingEvents->pop_front();
        }
        if (!result.success) {
            if (!VM_Exception_has_exception()) {
                VM_Exception_write("failed to run a hook on new event");
            }
            return;
        }
        // if (result.success) opcode = instr->original_opcode;
        // return result.success;
    }
}

/*
 * Methods that stop instrumentation
 */

void InstrumentationManager::stopRunningHooksOnNewEvents() {
    while (!CallbackHandler::pendingEvents->empty()) {
        // put pending events into main queue
        Event *ev = CallbackHandler::pendingEvents->front();
        CallbackHandler::pendingEvents->pop_front();
        CallbackHandler::push_event(ev);
    }
    CallbackHandler::pendingEventsActivated = false;

    Hooks_free_hooks(this->hooksForOnNewEvent);
}

void InstrumentationManager::stopRunningHooksOnEventsHandled() {
    this->interceptEvents = false;
    Hooks_free_hooks(this->hooksForOnEventHandling);
}

void InstrumentationManager::stopRunningHooksOnError() {
    this->interceptError = false;
    Hooks_free_hooks(this->hooksForOnError);
}

bool Instrumentation_interceptPrimitiveCall(Module *m) {
    return m->warduino->debugger->instrument.runHooksOnInterceptedFuncCall(
        *m->warduino->debugger->channel, m, &m->warduino->logicalClock,
        m->warduino->program_state);
}
