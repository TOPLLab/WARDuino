#pragma once

#include <queue>  // std::queue
#include <set>
#include <unordered_map>
#include <vector>

struct Module;

enum RunningState { WARDUINOrun, WARDUINOpause, WARDUINOstep };

enum InterruptTypes {
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
    interruptUPDATELocal = 0x21
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

    // Private methods

    // TODO Move parsing to WARDuino class?
    uint8_t *parseDebugBuffer(size_t len, const uint8_t *buff);

    //// Handle Interrupt Types

    void handleInterruptRUN(Module *m, RunningState *program_state);

    void handleInterruptBP(uint8_t *interruptData);

    //// Information dumps

    void dump(Module *m, bool full = false) const;

    void dumpLocals(Module *m) const;

    void dumpBreakpoints(Module *m) const;

    void dumpFunctions(Module *m) const;

    void dumpCallstack(Module *m) const;

    //// Handle live code update

    bool handleChangedFunction(Module *m, uint8_t *bytes);

    bool handleChangedLocal(Module *m, uint8_t *bytes) const;

   public:
    int socket;

    std::set<uint8_t *> breakpoints = {};  // Vector, we expect few breakpoints
    uint8_t *skipBreakpoint =
        nullptr;  // Breakpoint to skip in the next interpretation step

    explicit Debugger(int socket);

    // Interrupts

    void addDebugMessage(size_t len, const uint8_t *buff);

    uint8_t *getDebugMessage();

    bool checkDebugMessages(Module *m, RunningState *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void deleteBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);
};