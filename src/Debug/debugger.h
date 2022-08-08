#pragma once

#include <communication.pb.h>

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
    std::deque<communication::DebugMessage *> debugMessages = {};

    // Help variables

    std::vector<uint8_t> interruptBuffer;
    bool receivingData = false;

    Proxy *proxy = nullptr;  // proxy module for debugger

    bool connected_to_proxy = false;
    pthread_mutex_t supervisor_mutex;

    // Private methods

    void printValue(StackValue *v, uint32_t idx, bool end) const;

    StackValue *readRFCArgs(communication::RFC payload);

    //// Handle Interrupt Types

    void handleInterruptRUN(Module *m, communication::State *program_state);

    void handleInterruptBP(std::string breakpoint);

    //// Information dumps

    void dump(Module *m, bool snapshot = false) const;

    communication::Locals *captureLocals(Module *m) const;

    void captureBreakpoints(communication::Snapshot *snapshot) const;

    void captureFunctions(Module *m, communication::Snapshot *snapshot) const;

    void captureCallstack(Module *m, communication::Snapshot *snapshot) const;

    communication::EventsQueue *captureEventsQueue(
        const communication::Range &payload) const;

    void dumpCallbackmapping() const;

    //// Handle live code update

    static bool handleChangedFunction(Module *m,
                                      communication::Function payload);

    bool handleChangedLocal(Module *m, communication::Locals locals) const;

    //// Handle out-of-place debugging

    void freeState(Module *m, uint8_t *interruptData);

    static uint8_t *findOpcode(Module *m, Block *block);

    bool saveState(Module *m, uint8_t *interruptData);

    static uintptr_t readPointer(uint8_t **data);

    static void updateCallbackmapping(
        Module *m, const communication::CallbackMapping &mapping);

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

    communication::DebugMessage *getDebugMessage();

    bool checkDebugMessages(Module *m, communication::State *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void deleteBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);

    void notifyBreakpoint(uint8_t *pc_ptr) const;

    // Out-of-place debugging

    void proxify();

    // Pull-based

    void handleProxyCall(Module *m, communication::State *program_state,
                         const communication::RFC &payload);

    RFC *topProxyCall();

    void sendProxyCallResult(Module *m);

    bool isProxied(uint32_t fidx) const;

    void startProxySupervisor(int socket);

    bool proxy_connected() const;

    void disconnect_proxy();

    // Push-based

    void notifyPushedEvent() const;

    void handlePushedEvent(communication::Event payload) const;

    void loadState(Module *m, const communication::Snapshot &snapshot);
    void handleInterruptDumplocals(Module *m) const;
    void handleInterruptDumpevents(
        const communication::DebugMessage *message) const;
    bool wellformed(const communication::DebugMessage *message) const;
};
