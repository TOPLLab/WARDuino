#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>  // std::queue
#include <set>
#include <vector>

#include "../Debug/debug_msg.h"
#include "../Debug/runningstate.h"
#include "../Edward/proxy.h"
#include "../Edward/proxy_supervisor.h"
#include "../Instrumentation/instrumentation.h"
#include "../Utils/sockets.h"

// Error Codes
#define NO_ERROR 0
#define INTERRUPT_NR_DOES_NOT_MATCH_ERROR_CODE 1  // duplicate already
#define UPDATE_GLOBAL_VALUE_WRONG_INDEX_ERROR_CODE 90
#define UPDATE_STACK_VALUE_WRONG_INDEX_ERROR_CODE 91
#define UPDATE_STACK_VALUE_INVALID_DESERIALISE_ERROR_CODE 92
#define HANDLE_INVOKE_NO_FUNC_WITH_GIVEN_ID_ERROR_CODE 93
#define UPDATE_LOCAL_INVALID_INTERRUPT_NR 94
#define INVALID_BP_ADDR 99

class Debugger {
   private:
    std::deque<DebugMessage *> debugMessages = {};

    // Help variables
    volatile bool interruptWrite{};
    volatile bool interruptRead{};
    bool interruptEven = true;
    uint8_t interruptLastChar{};
    std::vector<uint8_t> interruptBuffer;
    std::queue<DebugMessage *> parsedInterrupts{};
    long interruptSize{};
    bool receivingData = false;

    Proxy *proxy = nullptr;  // proxy module for debugger

    bool connected_to_proxy = false;
    std::mutex *supervisor_mutex;

    // Private methods

    void printValue(StackValue *v, uint32_t idx, bool end) const;

    // TODO Move parsing to WARDuino class?
    void parseDebugBuffer(size_t len, const uint8_t *buff);

    void pushMessage(DebugMessage *msg);

    //// Handle REPL interrupts

    void handleInvoke(Module *m, uint8_t *interruptData);

    //// Handle Interrupt Types

    void handleInterruptRUN(DebugMessage *msg, Module *m,
                            RunningState *program_state);

    void handleSTEP(Module *m, RunningState *program_state);

    void handleSTEPOver(Module *m, RunningState *program_state);

    void handleInterruptBP(Module *m, uint8_t *interruptData);

    //// Information dumps

    void dump(Module *m, bool full = false) const;

    void dumpStack(Module *m) const;

    void dumpLocals(Module *m) const;

    void dumpBreakpoints(Module *m) const;

    void dumpFunctions(Module *m) const;

    void dumpCallstack(Module *m) const;

    void dumpCallbackmapping() const;

    void dumpHeapInfo(Module *m) const;

    //// Handle live code update

    static bool handleChangedFunction(Module *m, uint8_t *bytes);

    bool handleChangedLocal(Module *m, uint8_t *bytes) const;

    bool handleUpdateModule(Module *m, uint8_t *data);

    bool handleUpdateGlobalValue(Module *m, uint8_t *data);

    bool handleUpdateStackValue(Module *m, uint8_t *bytes);

    bool reset(Module *m);

    //// Handle out-of-place debugging

    void freeState(Module *m, uint8_t *interruptData);

    bool saveState(Module *m, uint8_t *interruptData);

    static uintptr_t readPointer(uint8_t **data);

    static void updateCallbackmapping(Module *m, const uint8_t *interruptData);

   public:
    // Public fields
    std::mutex messageQueueMutex;  // mutual exclude debugMessages
    std::condition_variable messageQueueConditionVariable;
    bool freshMessages = false;
    Channel *channel;
    ProxySupervisor *supervisor = nullptr;
    InstrumentationManager instrument;

    std::set<uint8_t *> breakpoints = {};  // Vector, we expect few breakpoints
    uint8_t *mark = 0;  // a unique temporary breakpoint that gets removed
                        // whenever a breakpoint is hit
    uint8_t *skipBreakpoint =
        nullptr;  // Breakpoint to skip in the next interpretation step

    // Constructor
    explicit Debugger(Channel *duplex);

    ~Debugger();

    void setChannel(Channel *duplex);

    // Public methods

    void stop();

    void pauseRuntime(Module *m);  // pause runtime for given module

    void dumpEvents(long start, long size) const;

    // Interrupts

    void addDebugMessage(size_t len, const uint8_t *buff);

    DebugMessage *getDebugMessage();

    bool checkDebugMessages(Module *m, RunningState *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void deleteBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);

    void notifyBreakpoint(Module *m, uint8_t *pc_ptr);

    void notifyStepCompleted();

    // Out-of-place debugging: EDWARD

    void snapshot(Module *m);

    void proxify();

    void handleProxyCall(Module *m, RunningState *program_state,
                         uint8_t *interruptData);

    RFC *topProxyCall();

    void sendProxyCallResult(Module *m);

    bool isProxied(uint32_t fidx) const;

    void startProxySupervisor(Channel *socket);

    bool proxy_connected() const;

    void disconnect_proxy();

    // Pull-based

    void handleMonitorProxies(Module *m, uint8_t *interruptData);

    // Push-based

    void notifyPushedEvent() const;

    bool handlePushedEvent(char *bytes) const;

    void handleAroundFunction(Module *m, uint8_t *data);

    void handleFuncCall(Module *m, uint8_t *data);

    void handleHookOnAddress(Module *m, uint8_t *data);

    void handleHookOnEvent(uint8_t *data);

    void handleHookOnError(uint8_t *data);
};
