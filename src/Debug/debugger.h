#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include "../Edward/proxy.h"
#include "../Edward/proxy_supervisor.h"
#include "../Threading/warduino-thread.h"
#include "../Utils/sockets.h"
#include "nanopb/debug.pb.h"
#include "nanopb/pb_decode.h"

struct Module;
struct Block;
struct StackValue;

struct DebugMessage {
    debug_Command type;
    std::vector<uint8_t> payload;
};

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
    heapState = 0x0D,
};

using SnapshotSelection = uint16_t;
enum SnapshotSection : SnapshotSelection {
    snapshotPc = 1u << 0,
    snapshotBreakpoints = 1u << 1,
    snapshotCallstack = 1u << 2,
    snapshotGlobals = 1u << 3,
    snapshotTable = 1u << 4,
    snapshotMemory = 1u << 5,
    snapshotBranchTable = 1u << 6,
    snapshotStack = 1u << 7,
    snapshotCallbacks = 1u << 8,
    snapshotEvents = 1u << 9,
    snapshotIO = 1u << 10,
    snapshotOverrides = 1u << 11,
    snapshotHeap = 1u << 12,
    snapshotFunctions = 1u << 13,
    snapshotLocals = 1u << 14
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
    interruptStored = 0xa1,
};

enum class SnapshotPolicy : int {
    none,                // Don't automatically take snapshots.
    atEveryInstruction,  // Take a snapshot after every instruction.
    checkpointing,       // Take a snapshot every x instructions or at specific
                         // points where primitives are used.
};

/*
 * FNV-1a 32bit:
 * https://datatracker.ietf.org/doc/html/draft-eastlake-fnv-17.html
 */
struct FNV1aVectorHash {
    size_t operator()(const std::vector<uint32_t> &values) const {
        constexpr uint32_t FNV_offset_basis = 0x811c9dc5;
        uint32_t result_hash = FNV_offset_basis;
        for (const uint32_t v : values) {
            for (int i = 0; i < 4; ++i) {
                constexpr uint32_t FNV_prime = 0x01000193;
                const uint8_t byte = (v >> (i * 8)) & 0xff;
                result_hash ^= byte;
                result_hash *= FNV_prime;
            }
        }
        return result_hash;
    }
};

class Debugger {
   private:
    std::deque<DebugMessage> debugMessages = {};

    // Incomplete bytes from the binary framed stream.
    std::vector<uint8_t> pendingFrameBytes;
    static constexpr size_t maxFramePayload = 65536;

    // Function replacement storage must outlive decoded queue frames.
    std::unordered_map<uint32_t, std::vector<uint8_t>> functionBodies;

    Proxy *proxy = nullptr;  // proxy module for debugger

    bool connected_to_proxy = false;
    warduino::mutex *supervisor_mutex;

    // Mocking
    std::unordered_map<std::vector<uint32_t>, uint32_t, FNV1aVectorHash>
        overrides;

    // Checkpointing
    SnapshotPolicy snapshotPolicy;
    uint32_t checkpointInterval;          // #instructions between checkpoints
    uint32_t instructions_executed;       // #instructions since last checkpoint
    std::optional<uint32_t> fidx_called;  // The primitive that was executed
    uint32_t prim_args[8];                // The arguments of the executed prim
    uint32_t min_return_values;
    uint32_t checkpoint_state_size;
    uint8_t *checkpoint_state;

    // Continue for
    int32_t remaining_instructions;

    // Private methods

    void print_value(const StackValue *v, uint32_t idx, bool end) const;

    // TODO Move parsing to WARDuino class?
    void parse_debug_buffer(size_t len, const uint8_t *buff);

    void push_message(DebugMessage msg);

    bool send_notification(debug_NotificationType type,
                          const pb_msgdesc_t *fields = nullptr,
                          const void *payload = nullptr) const;
    void send_operation_result(debug_Command command, bool success) const;

    //// Handle REPL interrupts

    void handle_invoke(Module *m, uint8_t *interruptData) const;

    //// Handle Interrupt Types

    void handle_interrupt_run(const Module *m, RunningState *program_state);

    void handle_step(const Module *m, RunningState *program_state);

    void handle_step_over(const Module *m, RunningState *program_state);

    void handle_interrupt_bp(Module *m, uint8_t *interruptData);

    //// Information dumps

    void dump(Module *m, bool full = false) const;

    void dump_stack(const Module *m) const;

    void dump_locals(const Module *m) const;

    void dump_breakpoints(Module *m) const;

    void dump_functions(Module *m) const;

    void dump_callstack(Module *m) const;

    void dump_events(long start, long size) const;

    void dump_callback_mapping() const;

    void dump_heap_info(Module *m) const;

    void inspect(Module *m, uint16_t sizeStateArray,
                 const uint8_t *state) const;
    bool encode_snapshot(Module *m, SnapshotSelection selection,
                        debug_NotificationType notification) const;
    static bool parse_selection(const uint8_t *state, size_t size,
                               SnapshotSelection *selection);

    //// Handle live code update

    static bool handle_changed_function(const Module *m, uint8_t *bytes);

    bool handle_changed_local(const Module *m, uint8_t *bytes) const;

    static bool handle_update_module(Module *m, uint8_t *data);

    bool handle_update_global_value(const Module *m, uint8_t *data) const;

    bool handle_update_stack_value(const Module *m, uint8_t *bytes) const;

    std::optional<debug_ValueUpdate> update_value(
        const std::vector<uint8_t> &payload) const;

    bool reset(Module *m);

    //// Handle mocking

    void add_override(Module *m, uint8_t *interruptData);
    void remove_override(Module *m, uint8_t *interruptData);

    //// Handle out-of-place debugging

    void free_state(Module *m, uint8_t *interruptData);

    static uint8_t *find_opcode(Module *m, const Block *block);

    bool save_state(Module *m, uint8_t *interruptData);

    static uintptr_t read_pointer(uint8_t **data);

    static void update_callback_mapping(Module *m, const char *interruptData);

    bool operation(Module *m, operation op);

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

    void set_channel(Channel *duplex);

    // Public methods

    void stop();

    void pause_runtime(const Module *m);  // pause runtime for given module

    void notify_complete_step(
        Module *m) const;  // notify the debugger frontend that a step was taken

    // Interrupts

    void add_debug_message(size_t len, const uint8_t *buff);

    std::optional<DebugMessage> get_debug_message();

    bool check_debug_messages(Module *m, RunningState *program_state);

    // Breakpoints

    void add_breakpoint(uint8_t *loc);

    void delete_breakpoint(uint8_t *loc);

    bool is_breakpoint(uint8_t *loc);

    void notify_breakpoint(Module *m, uint8_t *pc_ptr);

    // Out-of-place debugging: EDWARD

    void snapshot(Module *m) const;

    void set_snapshot_policy(Module *m, uint8_t *interruptData);

    void handle_snapshot_policy(Module *m);

    bool handle_continue_for(Module *m);

    void proxify();

    void handle_proxy_call(Module *m, RunningState *program_state,
                         uint8_t *interruptData) const;

    RFC *top_proxy_call() const;

    void send_proxy_call_result(Module *m) const;

    bool is_proxy() const;

    bool is_proxied(uint32_t fidx) const;

    void start_proxy_supervisor(Channel *socket);

    bool proxy_connected() const;

    void disconnect_proxy() const;

    // Pull-based

    void handle_monitor_proxies(const Module *m, uint8_t *interruptData) const;

    // Push-based

    void notify_pushed_event() const;

    bool handle_pushed_event(char *bytes) const;

    // Concolic Multiverse Debugging
    bool get_mock_for_args(Module *m, uint32_t fidx, uint32_t &result);

    // Checkpointing
    void checkpoint(Module *m, bool force = false);
    inline SnapshotPolicy get_snapshot_policy() { return snapshotPolicy; }
};
