#pragma once

#include <cstdint>
#include <queue>  // std::queue
#include <set>
#include <unordered_map>
#include <vector>
#ifndef ARDUINO
#include <thread>
#endif
struct Module;
struct Block;
struct StackValue;

enum RunningState {
    WARDUINOrun,
    WARDUINOpause,
    WARDUINOstep,
    WARDuinoProxyRun
};

enum InterruptTypes {
    // Remote Debugging
    interruptRUN = 0x01,
    interruptHALT = 0x02,
    interruptPAUSE = 0x03,
    interruptSTEP = 0x04,
    interruptBPAdd = 0x06,
    interruptBPRem = 0x07,
    interruptDUMP = 0x10,
    interruptDUMPLocals = 0x11,
    interruptDUMPFull = 0x12,
    interruptUPDATEFun = 0x20,
    interruptUPDATELocal = 0x21,

    // WOOD Pull Debugging
    interruptWOODDUMP = 0x60,
    interruptOffset = 0x61,
    interruptRecvState = 0x62,
    interruptMonitorProxies = 0x63,
    interruptProxyCall = 0x64,
    interruptDronify = 0x65,

    // Push Debugging
    interruptDUMPAllEvents = 0x70,
    interruptDUMPEvents = 0x71,
    interruptPOPEvent = 0x72,
    interruptPUSHEvent = 0x73,
    interruptDUMPCallbackmapping = 0x74,
    interruptRecvCallbackmapping = 0x75
};

class Debugger {
   private:
    std::deque<uint8_t *> debugMessages = {};

    // Help variables

    volatile bool interruptWrite{};
    volatile bool interruptRead{};
    bool interruptEven = true;
    uint8_t interruptLastChar{};
    std::vector<uint8_t> interruptBuffer;
    long interruptSize{};
    bool receivingData = false;

#ifndef ARDUINO
    bool connected_to_drone = false;
    pthread_mutex_t push_mutex;
    pthread_t push_debugging_threadid;
#endif

    // Private methods

    void printValue(StackValue *v, uint32_t idx, bool end) const;

    // TODO Move parsing to WARDuino class?
    uint8_t *parseDebugBuffer(size_t len, const uint8_t *buff);

    //// Handle Interrupt Types

    void handleInterruptRUN(Module *m, RunningState *program_state);

    void handleInterruptBP(const uint8_t *interruptData);

    //// Information dumps

    void dump(Module *m, bool full = false) const;

    void dumpLocals(Module *m) const;

    void dumpBreakpoints(Module *m) const;

    void dumpFunctions(Module *m) const;

    void dumpCallstack(Module *m) const;

    void dumpEvents(long start, long size) const;

    void dumpCallbackmapping() const;

    //// Handle live code update

    static bool handleChangedFunction(Module *m, uint8_t *bytes);

    bool handleChangedLocal(Module *m, uint8_t *bytes) const;

    //// Handle out-of-place debugging

    void freeState(Module *m, uint8_t *interruptData);

    static uint8_t *findOpcode(Module *m, Block *block);

    bool saveState(Module *m, uint8_t *interruptData);

    static uintptr_t readPointer(uint8_t **data);

    static void updateCallbackmapping(Module *m, const char *interruptData);
   public:
    // Public fields

    int socket;

    std::set<uint8_t *> breakpoints = {};  // Vector, we expect few breakpoints
    uint8_t *skipBreakpoint =
        nullptr;  // Breakpoint to skip in the next interpretation step

    // Constructor
    explicit Debugger(int socket);

    // Interrupts

    void addDebugMessage(size_t len, const uint8_t *buff);

    uint8_t *getDebugMessage();

    bool checkDebugMessages(Module *m, RunningState *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void deleteBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);

    void notifyBreakpoint(uint8_t *pc_ptr);

    // Out-of-place debugging

    void woodDump(Module *m);

#ifdef ARDUINO
    void handleProxyCall(Module *m, RunningState *program_state,
                         uint8_t *interruptData);
#else
    bool drone_connected() const;

    void disconnect_drone();

    // Pull-based

    void handleMonitorProxies(Module *m, uint8_t *interruptData);

    // Push-based

#ifndef ARDUINO
    void notifyPushedEvent() const;
#endif

    bool handlePushedEvent(Module *m, char *bytes) const;
#endif
};
