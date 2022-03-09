#pragma once

#include <queue>  // std::queue
#include <set>
#include <unordered_map>
#include <vector>

class Module;

enum RunningState { WARDUINOrun, WARDUINOpause, WARDUINOstep };

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

    void doDumpLocals(Module *m) const;

    void doDump(Module *m);

    bool readChange(Module *m, uint8_t *bytes);

    bool readChangeLocal(Module *m, uint8_t *bytes) const;

   public:
    int socket;

    std::set<uint8_t *> breakpoints = {};  // Vector, we expect few breakpoints
    uint8_t *skipBreakpoint =
        nullptr;  // Breakpoint to skip in the next interpretation step

    explicit Debugger(int socket);

    // Interrupts

    void addDebugMessage(
        size_t len, const uint8_t *buff);  // TODO Move parsing to WARDuino?

    uint8_t *getDebugMessage();

    bool checkDebugMessages(Module *m, RunningState *program_state);

    // Breakpoints

    void addBreakpoint(uint8_t *loc);

    void delBreakpoint(uint8_t *loc);

    bool isBreakpoint(uint8_t *loc);
};