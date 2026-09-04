#include "debugger-private.h"
#include "debugger-protocol.h"

// Debugger

Debugger::Debugger(Channel *duplex) {
    this->channel = duplex;
    this->supervisor_mutex = new warduino::mutex();
    this->supervisor_mutex->lock();
    this->snapshotPolicy = SnapshotPolicy::none;
    this->checkpointInterval = 10;
    this->instructions_executed = 0;
    this->fidx_called = {};
    this->min_return_values = 0;
    this->checkpoint_state = nullptr;
    this->checkpoint_state_size = 0;
    this->remaining_instructions = -1;
}

// Public methods

void Debugger::stop() {
    if (this->channel != nullptr) {
        this->channel->close();
        this->channel = nullptr;
    }
}

void Debugger::pause_runtime(const Module *m) {
    m->warduino->program_state = debug_State_STATE_WARDUINO_PAUSE;
    this->mark = nullptr;
}

void Debugger::notify_pushed_event() const {
    this->send_notification(debug_NotificationType_NOTIFICATION_NEW_EVENT);
}

void Debugger::set_channel(Channel *duplex) {
    delete this->channel;
    this->channel = duplex;
}

void Debugger::add_debug_message(const size_t len, const uint8_t *buff) {
    if (len == 0 || buff == nullptr) return;
    parse_debug_buffer(len, buff);
}

void Debugger::push_message(DebugMessage msg) {
    warduino::lock_guard const lg(messageQueueMutex);
    debugMessages.emplace_back(std::move(msg));
    freshMessages = !debugMessages.empty();
    messageQueueConditionVariable.notify_one();
}

void Debugger::parse_debug_buffer(const size_t len, const uint8_t *buff) {
    pendingFrameBytes.insert(pendingFrameBytes.end(), buff, buff + len);
    while (!pendingFrameBytes.empty()) {
        if (!is_known_command(pendingFrameBytes.front())) {
            pendingFrameBytes.clear();
            send_notification(
                debug_NotificationType_NOTIFICATION_UNKNOWN_COMMAND);
            continue;
        }

        size_t headerSize = 0;
        size_t payloadSize = 0;
        const bool completeLength =
            decode_frame_length(pendingFrameBytes, &headerSize, &payloadSize);
        if (!completeLength) {
            if (headerSize == SIZE_MAX || pendingFrameBytes.size() >= 6) {
                pendingFrameBytes.clear();
                send_notification(
                    debug_NotificationType_NOTIFICATION_MALFORMED);
            }
            return;
        }
        if (payloadSize > maxFramePayload) {
            pendingFrameBytes.clear();
            send_notification(debug_NotificationType_NOTIFICATION_MALFORMED);
            return;
        }
        if (pendingFrameBytes.size() < headerSize + payloadSize) return;

        DebugMessage message{static_cast<debug_Command>(pendingFrameBytes[0]),
                             {}};
        message.payload.assign(
            pendingFrameBytes.begin() + static_cast<std::ptrdiff_t>(headerSize),
            pendingFrameBytes.begin() +
                static_cast<std::ptrdiff_t>(headerSize + payloadSize));
        pendingFrameBytes.erase(
            pendingFrameBytes.begin(),
            pendingFrameBytes.begin() +
                static_cast<std::ptrdiff_t>(headerSize + payloadSize));
        push_message(std::move(message));
    }
}

std::optional<DebugMessage> Debugger::get_debug_message() {
    warduino::lock_guard const lg(messageQueueMutex);
    if (debugMessages.empty()) {
        freshMessages = false;
        return std::nullopt;
    }
    DebugMessage message = std::move(debugMessages.front());
    debugMessages.pop_front();
    freshMessages = !debugMessages.empty();
    return message;
}

bool Debugger::send_notification(const debug_NotificationType type,
                                 const pb_msgdesc_t *fields,
                                 const void *payload) const {
    if (channel == nullptr) return false;
    size_t payloadSize = 0;
    if (fields != nullptr && payload != nullptr &&
        !pb_get_encoded_size(&payloadSize, fields, payload)) {
        return false;
    }
    std::vector<uint8_t> frame;
    frame.reserve(1 + 5 + payloadSize);
    frame.push_back(static_cast<uint8_t>(type));
    size_t length = payloadSize;
    do {
        uint8_t byte = static_cast<uint8_t>(length & 0x7fU);
        length >>= 7U;
        if (length != 0) byte |= 0x80U;
        frame.push_back(byte);
    } while (length != 0);
    if (payloadSize != 0) {
        const size_t offset = frame.size();
        frame.resize(offset + payloadSize);
        pb_ostream_t stream =
            pb_ostream_from_buffer(frame.data() + offset, payloadSize);
        if (!pb_encode(&stream, fields, payload)) return false;
    }
    return channel->writeBytes(frame.data(), frame.size()) ==
           static_cast<ssize_t>(frame.size());
}

void Debugger::send_operation_result(const debug_Command command,
                                     const bool success) const {
    debug_OperationResult result = debug_OperationResult_init_zero;
    result.command = command;
    result.success = success;
    send_notification(debug_NotificationType_NOTIFICATION_OPERATION_RESULT,
                      debug_OperationResult_fields, &result);
}

void Debugger::add_breakpoint(uint8_t *loc) { this->breakpoints.insert(loc); }

void Debugger::delete_breakpoint(uint8_t *loc) { this->breakpoints.erase(loc); }

// ReSharper disable once CppParameterMayBeConstPtrOrRef // incorrect warning
bool Debugger::is_breakpoint(uint8_t *loc) {
    return this->breakpoints.find(loc) != this->breakpoints.end() ||
           this->mark == loc;
}

void Debugger::notify_breakpoint(Module *m, uint8_t *pc_ptr) {
    if (snapshotPolicy == SnapshotPolicy::checkpointing) checkpoint(m);
    mark = nullptr;
    debug_HitBreakpoint hit = debug_HitBreakpoint_init_zero;
    hit.has_location = true;
    hit.location.module_index = 0;
    hit.location.program_counter = toVirtualAddress(pc_ptr, m);
    send_notification(debug_NotificationType_NOTIFICATION_HIT_BREAKPOINT,
                      debug_HitBreakpoint_fields, &hit);
}

void Debugger::handle_interrupt_run(const Module *m,
                                    debug_State *program_state) {
    ExecutionContext *ectx = m->warduino->execution_context;
    if (*program_state == debug_State_STATE_WARDUINO_PAUSE &&
        this->is_breakpoint(ectx->pc_ptr)) {
        this->skipBreakpoint = ectx->pc_ptr;
    }
    *program_state = debug_State_STATE_WARDUINO_RUN;
}

void Debugger::handle_step(const Module *m, debug_State *program_state) {
    ExecutionContext *ectx = m->warduino->execution_context;
    *program_state = debug_State_STATE_WARDUINO_STEP;
    this->skipBreakpoint = ectx->pc_ptr;
}

void Debugger::handle_step_over(const Module *m, debug_State *program_state) {
    ExecutionContext *ectx = m->warduino->execution_context;
    this->skipBreakpoint = ectx->pc_ptr;
    uint8_t const opcode = *ectx->pc_ptr;
    if (opcode == 0x10) {  // step over direct call
        uint8_t *ptr_cpy = ectx->pc_ptr + 1;
        read_LEB_32(&ptr_cpy);
        this->mark = ectx->pc_ptr + (ptr_cpy - ectx->pc_ptr);
        *program_state = debug_State_STATE_WARDUINO_RUN;
        // warning: ack will be BP hit
    } else if (opcode == 0x11) {  // step over indirect call
        uint8_t *ptr_cpy = ectx->pc_ptr + 1;
        read_LEB_32(&ptr_cpy);
        read_LEB_32(&ptr_cpy);
        this->mark = ectx->pc_ptr + (ptr_cpy - ectx->pc_ptr);
        *program_state = debug_State_STATE_WARDUINO_RUN;
    } else {
        // normal step
        this->handle_step(m, program_state);
    }
}

bool Debugger::reset(Module *m) {
    m->warduino->reset_module(m);
    instructions_executed = 0;
    debug("Reset WARDuino.\n");
    return true;
}

bool Debugger::handle_continue_for(Module *m) {
    if (remaining_instructions < 0) return false;

    if (remaining_instructions == 0) {
        remaining_instructions = -1;
        if (snapshotPolicy == SnapshotPolicy::checkpointing) {
            checkpoint(m);
        }
        this->send_notification(debug_NotificationType_NOTIFICATION_PAUSED);
        pause_runtime(m);
        return true;
    }
    remaining_instructions--;
    return false;
}

void Debugger::notify_complete_step(Module *m) const {
    // Upon completing a step in checkpointing mode, make a checkpoint.
    if (m->warduino->debugger->get_snapshot_policy() ==
        SnapshotPolicy::checkpointing) {
        m->warduino->debugger->checkpoint(m);
    }
    this->send_notification(debug_NotificationType_NOTIFICATION_STEPPED);
}

Debugger::~Debugger() {
    this->disconnect_proxy();
    this->stop();
    delete this->supervisor_mutex;
    delete this->supervisor;
}
