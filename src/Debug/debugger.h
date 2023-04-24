#pragma once

#include <cstddef>
#include <cstdint>
#include <queue>  // std::queue
#include <set>
#include <unordered_map>
#include <vector>

#include "../Edward/proxy.h"
#include "../Edward/proxy_supervisor.h"
#include "../Utils/sockets.h"
#include "debug_structs.h"

#ifndef ARDUINO
#include <mutex>
#include <thread>
#endif

struct Module;
struct Block;
struct StackValue;

class Debugger {
   private:
    std::deque<uint8_t *> debugMessages = {};

    // Help variables
#ifndef ARDUINO
    std::mutex mutexDebugMsgs;  // mutual exclude debugMessages
#endif
    volatile bool interruptWrite{};
    volatile bool interruptRead{};
    bool interruptEven = true;
    uint8_t interruptLastChar{};
    std::vector<uint8_t> interruptBuffer;
    std::queue<uint8_t *> parsedInterrupts{};
    long interruptSize{};
    bool receivingData = false;

    Proxy *proxy = nullptr;  // proxy module for debugger

    bool connected_to_proxy = false;
    pthread_mutex_t supervisor_mutex;

    // Private methods

    void printValue(StackValue *v, uint32_t idx, bool end) const;

    // TODO Move parsing to WARDuino class?
    void parseDebugBuffer(size_t len, const uint8_t *buff);

    void pushMessage(uint8_t *msg);

    //// Handle REPL interrupts

    static void handleInvoke(Module *m, uint8_t *interruptData);

    //// Handle Interrupt Types

    void handleInterruptRUN(Module *m, RunningState *program_state);

    void handleInterruptBP(Module *m, uint8_t *interruptData);

    //// Information dumps

    void dump(Module *m, bool full = false) const;

    void dumpLocals(Module *m) const;

    void dumpBreakpoints() const;

    void dumpFunctions(Module *m) const;

    void dumpCallstack(Module *m) const;

    void dumpEvents(long start, long size) const;

    void dumpCallbackmapping() const;

    //// Handle live code update

    static bool handleChangedFunction(Module *m, uint8_t *bytes);

    bool handleChangedLocal(Module *m, uint8_t *bytes) const;

    bool handleUpdateStackValue(Module *m, uint8_t *bytes);

    bool handleUpdateGlobalValue(Module *m, uint8_t *data);

    bool handleUpdateModule(Module *m, uint8_t *data);

    //// Handle out-of-place debugging

    void freeState(Module *m, uint8_t *interruptData);

    static uint8_t *findOpcode(Module *m, Block *block);

    bool saveState(Module *m, uint8_t *interruptData);

    static uintptr_t readPointer(uint8_t **data);

    static void updateCallbackmapping(Module *m, const char *interruptData);

   public:
    // Public fields
    Channel *channel{};
    Channel *proxyChannel{};
    ProxySupervisor *supervisor = nullptr;

    std::set<uint8_t *> breakpoints = {};  // Vector, we expect few breakpoints
    uint8_t *skipBreakpoint =
        nullptr;  // Breakpoint to skip in the next interpretation step

    // Constructor
    explicit Debugger(Channel *duplex);

    void setChannel(Channel *duplex);

    void setProxyChannel(Channel *duplex);

    // Public methods

    void stop();

    // Interrupts

    void addDebugMessage(size_t len, const uint8_t *buff);

    uint8_t *getDebugMessage();

    bool checkDebugMessages(Module *m, RunningState *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void deleteBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);

    void notifyBreakpoint(uint32_t bp) const;

    // Out-of-place debugging

    void dumpAllState(Module *m);

    void dumpExecutionState(Module *m, uint16_t sizeStateArray, uint8_t *state);

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

    // move to seperate serialisation/deserialisation file
    bool deserialiseStackValueIntoDest(StackValue *dest, uint8_t *bytes);
};
