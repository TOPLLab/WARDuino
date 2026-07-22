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

    uint32_t step_id = 0;

    // Private methods

    void printValue(StackValue *v, uint32_t idx, bool end) const;

    // TODO Move parsing to WARDuino class?
    void parseDebugBuffer(size_t len, const uint8_t *buff);

    void pushMessage(DebugMessage *msg);

    //// Handle REPL interrupts

    void handleInvoke(Module *m, DebugMessage *msg);

    //// Handle Interrupt Types

    void handleInterruptRUN(DebugMessage *msg, Module *m,
                            RunningState *program_state);

    void handleSTEP(DebugMessage *msg, Module *m, RunningState *program_state);

    void handleSTEPOver(DebugMessage *msg, Module *m,
                        RunningState *program_state);

    void handleInterruptBP(Module *m, DebugMessage *msg);

    //// Information dumps

    void dump(Module *m, bool full = false, bool newline = true) const;

    void handleDump(Module *m, DebugMessage *msg, bool full = false) const;

    void dumpStack(Module *m, bool newline = true) const;

    void dumpLocals(Module *m) const;

    void handleDumpLocals(Module *m, DebugMessage *msg) const;

    void dumpBreakpoints(Module *m) const;

    void dumpFunctions(Module *m) const;

    void dumpCallstack(Module *m) const;

    void dumpCallbackmapping(DebugMessage *msg) const;

    void dumpHeapInfo(Module *m) const;

    void handleDumpEvents(DebugMessage *msg, long start = 0, long size = 0);

    //// Handle live code update

    static bool handleChangedFunction(Module *m, DebugMessage *msg);

    bool handleChangedLocal(Module *m, DebugMessage *msg) const;

    bool handleUpdateModule(Module *m, DebugMessage *msg);

    bool handleUpdateGlobalValue(Module *m, DebugMessage *msg);

    bool handleUpdateStackValue(Module *m, DebugMessage *msg);

    bool reset(Module *m, DebugMessage *msg);

    //// Handle out-of-place debugging

    void freeState(Module *m, uint8_t *interruptData);

    bool saveState(Module *m, uint8_t *interruptData);

    static uintptr_t readPointer(uint8_t **data);

    static void updateCallbackmapping(Module *m, DebugMessage *msg);

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

    void loadSnapshot(Module *m, DebugMessage *msg);

    void handleSnapshot(Module *m, DebugMessage *msg);

    void snapshot(Module *m, bool includeHeader = true,
                  bool includeNewline = true);

    void proxify(DebugMessage *msg);

    void handleProxyCall(Module *m, uint8_t *interruptData);

    RFC *topProxyCall();

    void sendProxyCallResult(Module *m);

    bool isProxied(uint32_t fidx) const;

    void startProxySupervisor(Channel *socket);

    bool proxy_connected() const;

    void disconnect_proxy();

    // Pull-based

    void handleMonitorProxies(Module *m, DebugMessage *msg);

    // Push-based

    void notifyPushedEvent() const;

    bool handlePushedEvent(DebugMessage *msg) const;

    void handleAroundFunction(Module *m, uint8_t *data);

    void handleFuncCall(Module *m, uint8_t *data);

    void handleHookOnAddress(Module *m, uint8_t *data);

    void handleHookOnEvent(uint8_t *data);

    void handleHookOnError(DebugMessage *msg);
};
