#pragma once
#include <cstdint>
#include <stack>
#include <unordered_map>

#include "../Instrumentation/logical_clock.h"
#include "../Interrupts/interrupt_hook_on_event_struct.h"
#include "../Utils/sockets.h"
#include "../WARDuino/event_structs.h"
#include "../WARDuino/structs.h"
#include "./hook.h"
#include "./instrumentation_structs.h"

#define INSTRUMENTATION_INTERCEPT_OPCODE 0xff

typedef struct MonitoredFrame {
    uint32_t addr{0};
    int frame_idx{-1};
} MonitoredFrame;

enum HookRunResult {
    HookCompleted,
    HookFailed,
    HookDelayed,
};

typedef struct HooksResult {
    Hook *hooks_left{};
    bool one_delayed{};
    bool success{};
    uint8_t hook_idx{};
} HooksResult;

typedef struct HookArgs {
    LogicalClock &currentTime;
    uint32_t addr;
    RunningState &runningState;
    uint32_t local_fidx;

    void (*hookOnAddressSubContent)(const Channel &, HookMoment, uint32_t,
                                    uint32_t, bool);
    void (*hookOnErrorSubContent)(const Channel &, uint32_t, bool);
    void (*hookOnEventSubContent)(const Channel &, uint32_t, HookEventMoment,
                                  bool);
    HookMoment moment;
    HookEventMoment eventMoment;
    Event *ev;
} HookArgs;

class InstrumentationManager {
   private:
    LogicalClock lastObservedTime{};

    Channel *fun_call_channel{};

    std::stack<MonitoredFrame> frames_to_monitor{};

    std::unordered_map<uint32_t, HooksPrimitiveFunc *>
        hooks_around_prim_funcs{};

    std::unordered_map<uint32_t, HooksWasmAddr *> instr_wasm_addr_before{};

    std::unordered_map<uint32_t, HooksWasmAddr *> instr_wasm_addr_after{};

    Hook *hooksForOnNewEvent{};

    Hook *hooksForOnEventHandling{};

    Hook *hooksForOnError{};

    HooksPrimitiveFunc *new_Primitive_Instrumentation();

    void delete_Primitive_Instrumentation(HooksPrimitiveFunc *func);

    HooksWasmAddr *new_WasmAddress_Instrumentation();

    bool do_remote_call(Channel &channel, Module *m, uint32_t local_fidx,
                        uint32_t func_to_call, bool isProxyCall);

    void run_hooks(const Channel &output, Module &module, Hook *hooks,
                   HookArgs &args, HooksResult &result);

    HookRunResult run_hook(const Channel &output, Module &module, Hook &hook,
                           HookArgs &args);

    bool do_value_substitution(Module *module, uint32_t func_called,
                               Hook *hook);

    bool do_before_wasm_addr_hooks(const Channel &hookOutput, Module &module,
                                   LogicalClock &currentTime, uint32_t addr,
                                   uint8_t &opcode, RunningState &runningState);

    /*
     * Methods that start instrumentation
     */

    HooksPrimitiveFunc *start_primitive_call_interception(Module &m,
                                                          uint32_t target_func);

    HooksWasmAddr *start_wasm_addr_intercept(Module &module,
                                             const uint32_t addr,
                                             const HookMoment moment);

    /*
     * Methods that stop instrumentation
     */

    bool stop_primitive_call_interception(Module &m, uint32_t target_func);

    void stopRunningHooksOnNewEvents();

    void stopRunningHooksOnEventsHandled();

    void stopRunningHooksOnError();

    uint8_t getOriginalOpcode(Module &module, uint32_t addr);

   public:
    bool awakeOnNextInstruction = false;
    bool interceptEvents = false;
    bool interceptError = false;

    InstrumentationManager();

    void registerAroundFunctionChannel(Channel *channel);

    /*
     * Hook registration methods
     */

    bool addHookAroundFunction(Module &m, uint32_t func_idx,
                               const Hook &around);

    bool removeHooksAroundFunction(Module &m, uint32_t func_idx);

    bool addHookOnWasmAddress(Module &module, uint32_t addr, Hook &hook,
                              const HookMoment moment);

    bool removeHooksOnWasmAddress(Module &module, uint32_t addr,
                                  const HookMoment moment);

    bool addHookOnNewEvent(Hook &hook);

    bool addHookOnEventHandling(Hook &hook);

    bool addHookOnError(Hook &hook);

    /*
     *  Predicate methods
     */
    bool has_HooksOnAroundFunction(uint32_t funID);

    bool has_HookOnWasmAddr(uint32_t addr, HookMoment moment);

    bool isAddHookAroundFuncAllowed(uint32_t funID);

    bool isAddHookOnEventAllowed(Hook &hook);

    bool isAddHookOnEventHandlingAllowed(Hook &hook);

    bool isAddHookOnErrorAllowed(Hook &hook);

    /*
     * Running hooks methods
     */

    /* method called by the CallbackHandler to give control the
     * Instrumentration object for intercepting events that will be handled.
     *
     * returns bool :  if true the Instrumentation wants to continue
     * handling intercepting events after the call false the. If false
     * control is given back to the callback handler
     */
    bool runHookForOnEventHandling(const Channel &output, Module *module);

    void runHooksForOnNewEvent(const Channel &output, Module *module);

    void runHooksOnError(const Channel &output, Module *module,
                         LogicalClock *currentTime);

    bool runHooksAfterWasmAddr(const Channel &output, Module *module,
                               RunningState &runningState);

    bool runHooksOnInterceptedFuncCall(const Channel &output, Module *module,
                                       LogicalClock *currentTime,
                                       RunningState &runningState);

    bool runHooksOnWasmAddr(const Channel &output, Module *module,
                            LogicalClock *currentTime, uint8_t &opcode,
                            RunningState &runningState);
};

bool Instrumentation_interceptPrimitiveCall(Module *module);