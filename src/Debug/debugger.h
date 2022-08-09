#pragma once

#include <debug.pb.h>

#include <cstddef>
#include <cstdint>
#include <queue>  // std::queue
#include <set>
#include <unordered_map>
#include <vector>

#include "../Edward/proxy.h"
#include "../Edward/proxy_supervisor.h"
#include "../Utils/sockets.h"

#ifndef ARDUINO
#include <thread>
#endif
struct Module;
struct Block;
struct StackValue;

class Debugger {
   private:
    std::deque<debug::DebugMessage *> debugMessages = {};

    // Help variables

    std::vector<uint8_t> interruptBuffer;
    bool receivingData = false;

    Proxy *proxy = nullptr;  // proxy module for debugger

    bool connected_to_proxy = false;
    pthread_mutex_t supervisor_mutex;

    // Private methods

    void printValue(StackValue *v, uint32_t idx, bool end) const;

    StackValue *readRFCArgs(debug::RFC payload);

    bool wellformed(const debug::DebugMessage *message) const;

    //// Handle Interrupt Types

    void sendSimpleNotification(debug::Notification_Type type);

    void handleInterruptRUN(Module *m, debug::State *program_state);

    void handleInterruptHALT();

    void handleInterruptPAUSE(Module *m, debug::State *program_state);

    void handleInterruptSTEP(Module *m, debug::State *program_state);

    void handleInterruptBP(std::string breakpoint);

    //// Information dumps

    void handleInterruptDumplocals(Module *m) const;

    void handleInterruptDumpevents(const debug::DebugMessage *message) const;

    void dump(Module *m, bool snapshot = false) const;

    debug::Locals *captureLocals(Module *m) const;

    void captureBreakpoints(debug::Snapshot *snapshot) const;

    void captureFunctions(Module *m, debug::Snapshot *snapshot) const;

    void captureCallstack(Module *m, debug::Snapshot *snapshot) const;

    debug::EventsQueue *captureEventsQueue(const debug::Range &payload) const;

    void dumpCallbackmapping() const;

    //// Handle live code update

    static bool handleChangedFunction(Module *m, debug::Function payload);

    bool handleChangedLocal(Module *m, debug::Locals locals) const;

    //// Handle out-of-place debugging

    void freeState(Module *m, uint8_t *interruptData);

    static uint8_t *findOpcode(Module *m, Block *block);

    bool saveState(Module *m, uint8_t *interruptData);

    static uintptr_t readPointer(uint8_t **data);

    static void updateCallbackmapping(Module *m,
                                      const debug::CallbackMapping &mapping);

   public:
    // Public fields
    Channel *channel;
    ProxySupervisor *supervisor = nullptr;

    std::set<uint8_t *> breakpoints = {};  // Vector, we expect few breakpoints
    uint8_t *skipBreakpoint =
        nullptr;  // Breakpoint to skip in the next interpretation step

    // Constructor
    explicit Debugger(int address);

    void setChannel(int address);

    // Interrupts

    void addDebugMessage(size_t len, const uint8_t *buff);

    debug::DebugMessage *getDebugMessage();

    bool checkDebugMessages(Module *m, debug::State *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void deleteBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);

    void notifyBreakpoint(uint8_t *pc_ptr) const;

    // Out-of-place debugging

    void proxify();

    // Pull-based

    void handleProxyCall(Module *m, debug::State *program_state,
                         const debug::RFC &payload);

    RFC *topProxyCall();

    void sendProxyCallResult(Module *m);

    bool isProxied(uint32_t fidx) const;

    void startProxySupervisor(int socket);

    bool proxy_connected() const;

    void disconnect_proxy();

    // Push-based

    void notifyPushedEvent() const;

    void handlePushedEvent(debug::Event payload) const;

    void loadState(Module *m, const debug::Snapshot &snapshot);
};
