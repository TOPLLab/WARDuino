#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>  // std::queue
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../Oop/proxy.h"
#include "../Threading/warduino-thread.h"
#include "../Utils/sockets.h"

struct Module;
struct Block;
struct StackValue;

enum operation {
    STORE = 0,
    LOAD = 1,
};

enum RunningState {
    WARDUINOinit,
    WARDUINOrun,
    WARDUINOpause,
    WARDUINOstep,
    PROXYrun,  // Running state used when executing a proxy call. During
               // this state the call is set up and executed by the main
               // loop. After execution, the state is restored to
               // PROXYhalt
    PROXYhalt  // Do not run the program (program runs on computer, which
               // sends messages for primitives, do forward interrupts)
};

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
    ioState = 0x0B,
    overridesState = 0x0C,
};

enum InterruptTypes {
    // Remote Debugging
    interruptRUN = 0x01,
    interruptHALT = 0x02,
    interruptPAUSE = 0x03,
    interruptSTEP = 0x04,
    interruptSTEPOver = 0x05,
    interruptBPAdd = 0x06,
    interruptBPRem = 0x07,
    interruptContinueFor = 0x08,
    interruptInspect = 0x09,
    interruptDUMP = 0x10,
    interruptDUMPLocals = 0x11,
    interruptDUMPFull = 0x12,
    interruptReset = 0x13,
    interruptUPDATEFun = 0x20,
    interruptUPDATELocal = 0x21,
    interruptUPDATEModule = 0x22,
    interruptUPDATEGlobal = 0x23,
    interruptUPDATEStackValue = 0x24,

    // Remote REPL
    interruptINVOKE = 0x40,

    // Stateful out-of-place
    interruptTransfer = 0x52,

    // Pull Debugging
    interruptSnapshot = 0x60,
    interruptSetSnapshotPolicy = 0x61,
    interruptLoadSnapshot = 0x62,
    interruptMonitorProxies = 0x63,
    interruptProxyCall = 0x64,
    interruptProxify = 0x65,  // wifi SSID \0 wifi PASS \0

    // Push Debugging
    interruptDUMPAllEvents = 0x70,
    interruptDUMPEvents = 0x71,
    interruptPOPEvent = 0x72,
    interruptPUSHEvent = 0x73,
    interruptDUMPCallbackmapping = 0x74,
    interruptRecvCallbackmapping = 0x75,

    // Primitive overrides
    interruptSetOverridePinValue = 0x80,
    interruptUnsetOverridePinValue = 0x81,

    // Operations
    interruptStore = 0xa0,
};

enum class SnapshotPolicy : int {
    none,                // Don't automatically take snapshots.
    atEveryInstruction,  // Take a snapshot after every instruction.
    checkpointing,       // Take a snapshot every x instructions or at specific
                         // points where primitives are used.
};

class ProxySupervisor;

class Debugger {
   private:
    std::deque<uint8_t *> debugMessages = {};

    // Help variables
    volatile bool interruptWrite{};
    volatile bool interruptRead{};
    bool interruptEven = true;
    uint8_t interruptLastChar{};
    std::vector<uint8_t> interruptBuffer;
    std::queue<uint8_t *> parsedInterrupts{};
    long interruptSize{};
    bool receivingData = false;
public:
    Proxy *proxy = nullptr;  // proxy module for debugger

    bool connected_to_proxy = false;
    warduino::mutex *supervisor_mutex;

    // Mocking
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>>
        overrides;

    // Checkpointing
    SnapshotPolicy snapshotPolicy;
    uint32_t checkpointInterval;          // #instructions between checkpoints
    uint32_t instructions_executed;       // #instructions since last checkpoint
    std::optional<uint32_t> fidx_called;  // The primitive that was executed
    uint32_t prim_args[8];                // The arguments of the executed prim

    // Continue for
    int32_t remaining_instructions;

    // Private methods

    void printValue(const StackValue *v, uint32_t idx, bool end) const;

    // TODO Move parsing to WARDuino class?
    void parseDebugBuffer(size_t len, const uint8_t *buff);

    void pushMessage(uint8_t *msg);

    //// Handle REPL interrupts

    void handleInvoke(Module *m, uint8_t *interruptData) const;

    //// Handle Interrupt Types

    void handleInterruptRUN(const Module *m, RunningState *program_state);

    void handleSTEP(Module *m, RunningState *program_state);

    void handleSTEPOver(Module *m, RunningState *program_state);

    void handleInterruptBP(Module *m, uint8_t *interruptData);

    //// Information dumps

    void dump(Module *m, bool full = false) const;

    void dumpStack(const Module *m) const;

    void dumpLocals(const Module *m) const;

    void dumpBreakpoints(Module *m) const;

    void dumpFunctions(Module *m) const;

    void dumpCallstack(Module *m) const;

    void dumpEvents(long start, long size) const;

    void dumpCallbackmapping() const;

    void inspect(Module *m, uint16_t sizeStateArray,
                 const uint8_t *state) const;

    //// Handle live code update

    static bool handleChangedFunction(const Module *m, uint8_t *bytes);

    bool handleChangedLocal(const Module *m, uint8_t *bytes) const;

    static bool handleUpdateModule(Module *m, uint8_t *data);

    bool handleUpdateGlobalValue(const Module *m, uint8_t *data) const;

    bool handleUpdateStackValue(const Module *m, uint8_t *bytes) const;

    bool reset(Module *m) const;

    //// Handle out-of-place debugging

    void freeState(Module *m, uint8_t *interruptData);

    static uint8_t *findOpcode(Module *m, const Block *block);

    bool saveState(Module *m, uint8_t *interruptData);

    void load(uint8_t *bytes, Module *m);

    void transfer(Module *m, uint8_t *interruptData);

    static uintptr_t readPointer(uint8_t **data);

    static void updateCallbackmapping(Module *m, const char *interruptData);

    void receiveStore(Module *m, uint8_t *interruptData);

   public:
    // Public fields
    warduino::mutex messageQueueMutex;  // mutual exclude debugMessages
    warduino::condition_variable messageQueueConditionVariable;
    bool freshMessages = false;
    Channel *channel;
    ProxySupervisor *supervisor = nullptr;

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

    void pauseRuntime(const Module *m);  // pause runtime for given module

    void notifyCompleteStep(
        Module *m) const;  // notify the debugger frontend that a step was taken

    // Interrupts

    void addDebugMessage(size_t len, const uint8_t *buff);

    uint8_t *getDebugMessage();

    bool checkDebugMessages(Module *m, RunningState *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void deleteBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);

    void notifyBreakpoint(Module *m, uint8_t *pc_ptr);

    // Out-of-place debugging: EDWARD

    void snapshot(Module *m) const;

    void setSnapshotPolicy(Module *m, uint8_t *interruptData);

    void handleSnapshotPolicy(Module *m);

    bool handleContinueFor(Module *m);

    void proxify();

    void handleProxyCall(Module *m, RunningState *program_state,
                         uint8_t *interruptData) const;

    RFC *topProxyCall() const;

    void sendProxyCallResult(Module *m) const;

    bool isProxy() const;

    bool isProxied(Module *m, uint32_t fidx) const;

    void startProxySupervisor(Channel *socket);

    bool proxy_connected() const;

    void disconnect_proxy() const;

    // Pull-based

    void handleMonitorProxies(const Module *m, uint8_t *interruptData) const;

    // Push-based

    void notifyPushedEvent() const;

    bool handlePushedEvent(char *bytes) const;

    // Concolic Multiverse Debugging
    inline bool isMocked(uint32_t fidx, uint32_t argument) {
        return overrides.count(fidx) > 0 && overrides[fidx].count(argument) > 0;
    }
    inline uint32_t getMockedValue(uint32_t fidx, uint32_t argument) {
        return overrides[fidx][argument];
    }

    void addOverride(Module *m, uint8_t *interruptData);
    void removeOverride(Module *m, uint8_t *interruptData);

    // Checkpointing
    void checkpoint(Module *m, bool force = false);
    inline SnapshotPolicy getSnapshotPolicy() { return snapshotPolicy; }
};
