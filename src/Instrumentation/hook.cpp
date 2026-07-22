#include "hook.h"

#include <cstring>

#include "../Utils/util.h"
#include "logical_clock.h"
#include "schedule.h"

/*
 * Declaration private functions
 */
bool Hooks_deserialize_hook_rest(Hook &dest, uint8_t **encoded_hook,
                                 uint8_t &error_code);

/*
 * Public functions
 */
Hook *Hooks_add_and_sort(Hook *hooks, Hook *hook_to_add) {
    if (hook_to_add == nullptr) {
        return hooks;
    } else if (hooks == nullptr) {
        return hook_to_add;
    }

    // TODO sort hooks from more specific to more general
    // sorting goes from logical-clock, ev dep, cond, once, always
    // logical-clock < logical-clock < .. < logical-clock
    // -> sub sort ts before(s) .... ts on ts .... ts after
    // event dependency < event dep. < ... < event dep. condition
    // Once < once < ... < once
    // always (only one allowed)
    // BUT FOR NOW: just add hook at the end
    Hook *insert = hooks;
    while (insert->nextHook != nullptr) {
        insert = insert->nextHook;
    }
    if (insert->nextHook != nullptr) {
        hook_to_add->nextHook = insert->nextHook;
    }
    insert->nextHook = hook_to_add;
    return hooks;
}

Hook *Hooks_remove_completed_hook(Hook *hooks) {
    Hook *first_hook = hooks;
    Hook *prev_hook = nullptr;
    Hook *current_hook = first_hook;
    while (current_hook != nullptr) {
        Hook *nc = nullptr;
        bool remove = false;
        switch (current_hook->schedule.kind) {
            case ScheduleAlways:
                break;
            case ScheduleBeforeLogicalClock:
                remove = LogicalClock_is_t1_smaller_t2(
                    WARDuino::instance()->logicalClock,
                    current_hook->schedule.value.logicalClock);
                break;
            case ScheduleOnLogicalClock:
                remove = LogicalClock_is_t1_equal_t2(
                    WARDuino::instance()->logicalClock,
                    current_hook->schedule.value.logicalClock);
                break;
            case ScheduleAfterLogicalClock:
                remove = LogicalClock_is_t1_greater_t2(
                    WARDuino::instance()->logicalClock,
                    current_hook->schedule.value.logicalClock);
                break;
            case ScheduleOnce: {
                remove = true;
                break;
            }
        }

        if (remove) {
            nc = current_hook->nextHook;
            Hooks_free_hook(current_hook);
            current_hook = nc;
            if (prev_hook == nullptr) {
                first_hook = nc;
            } else {
                prev_hook->nextHook = current_hook;
            }
        } else {
            prev_hook = current_hook;
            current_hook = current_hook->nextHook;
        }
    }
    return first_hook;
}

bool Hook_isScheduledForNow(LogicalClock ct, Schedule s) {
    switch (s.kind) {
        case ScheduleBeforeLogicalClock:
            return LogicalClock_is_t1_smaller_t2(ct, s.value.logicalClock);
        case ScheduleAfterLogicalClock:
            return LogicalClock_is_t1_greater_t2(ct, s.value.logicalClock);
        case ScheduleOnLogicalClock:
            return LogicalClock_is_t1_equal_t2(ct, s.value.logicalClock);
        default:
            return true;
    }
}

bool Hooks_isHookWaitingForEvent(Hook *sorted_hooks,
                                 const LogicalClock &currentTime) {
    Hook *hook = sorted_hooks;
    while (hook != nullptr) {
        // TODO decide: whether to move the code here to Sceduler.h
        switch (hook->schedule.kind) {
            case ScheduleOnLogicalClock:
            case ScheduleAfterLogicalClock:
            case ScheduleBeforeLogicalClock:
                return hook->schedule.value.logicalClock.nr_of_events >
                           currentTime.nr_of_events &&
                       hook->schedule.value.logicalClock.nr_of_instructions >=
                           currentTime.nr_of_instructions;
            default:
                hook = hook->nextHook;
        }
    }
    return false;
}

Hook *Hooks_copyHook(const Hook &hook) {
    Hook *cpy = new Hook();
    if (cpy != nullptr) {
        cpy->kind = hook.kind;
        cpy->schedule = hook.schedule;
        switch (cpy->kind) {
            case ProxyCall:
            case RemoteCall:
                cpy->value.target_fidx = hook.value.target_fidx;
                break;
            case ValueSubstitution:
                if (hook.value.result != nullptr) {
                    cpy->value.result = new StackValue;
                    if (cpy->value.result == nullptr) {
                        delete cpy;
                        cpy = nullptr;
                    } else {
                        cpy->value.result->value = hook.value.result->value;
                        cpy->value.result->value_type =
                            hook.value.result->value_type;
                    }
                }
                break;
            case StateInspect:
                cpy->value.state = Interrupt_Inspect_new_state_to_inspect();
                if (!Interrupt_Inspect_copy_state_to_inspect(
                        *cpy->value.state, *hook.value.state)) {
                    delete cpy;
                    cpy = nullptr;
                }
                Interrupt_Inspect_free_state_to_inspect(hook.value.state);
                break;
            case ChangeRunningState:
                cpy->value.runState = hook.value.runState;
                break;
            case EventAdd: {
                cpy->value.ev = hook.value.ev;
                break;
            }
            default:
                return nullptr;
        }
    }
    cpy->id = hook.id;
    return cpy;
}

void Hooks_free_hook(Hook *hook) {
    hook->nextHook = nullptr;
    if (hook->kind == ValueSubstitution && hook->value.result != nullptr) {
        delete hook->value.result;
    }
    delete hook;
}

void Hooks_free_hooks(Hook *hook) {
    while (hook != nullptr) {
        Hook *hookToFree = hook;
        hook = hook->nextHook;
        Hooks_free_hook(hookToFree);
    }
}

/*
 * Private functions
 */
bool Hooks_deserialize_hook(Hook &dest, uint32_t id, uint8_t **encoded_hook,
                            uint8_t &error_code) {
    // format expected: Schedule | Hook
    dest.id = id;
    return Hooks_deserialize_schedule(dest.schedule, encoded_hook,
                                      error_code) &&
           Hooks_deserialize_hook_rest(dest, encoded_hook, error_code);
}

bool Hooks_deserialize_schedule(Schedule &dest, uint8_t **encoded_schedule,
                                uint8_t &error_code) {
    // format expected: SCHEDULE_KIND (1 BYTE)
    // format logical-clock: nr of instructions (LEB32) | nr of events
    // (LEB32);

    ScheduleKind schedule = (ScheduleKind) * *encoded_schedule;
    *encoded_schedule += 1;
    switch (schedule) {
        case ScheduleOnce:
        case ScheduleAlways:
            break;
        case ScheduleOnLogicalClock:
        case ScheduleBeforeLogicalClock:
        case ScheduleAfterLogicalClock:
            dest.value.logicalClock.nr_of_instructions =
                read_LEB_32(encoded_schedule);
            dest.value.logicalClock.nr_of_events =
                read_LEB_32(encoded_schedule);
            break;
        default:
            error_code = HOOK_ERROR_CODE_UNEXISTING_SCHEDULE_KIND;
            return false;
    }
    dest.kind = schedule;
    return true;
}

bool Hooks_deserialize_hook_rest(Hook &dest, uint8_t **encoded_hook,
                                 uint8_t &error_code) {
    HookKind kind = (HookKind) * *encoded_hook;
    *encoded_hook += 1;
    switch (kind) {
        case ProxyCall:
            break;
        case RemoteCall:
            dest.value.target_fidx = read_LEB_32(encoded_hook);
            break;
        case ValueSubstitution: {
            bool hasValue = **encoded_hook;
            *encoded_hook += 1;
            if (hasValue) {
                ValueSerializationConfig config;
                config.includeType = true;
                config.includeIndex = false;
                size_t bytes_read = deserializeStackValue(
                    dest.value.result, config, *encoded_hook);
                if (bytes_read <= 0) {
                    error_code = HOOK_ERROR_CODE_SUBSTITUTE_VALUE_IS_MALFORMED;
                    return false;
                }
            } else {
                dest.value.result = nullptr;
            }
            break;
        }
        case StateInspect:
            // TODO unfiromly malloc or free during deserialization
            dest.value.state = Interrupt_Inspect_new_state_to_inspect();
            if (dest.value.state == nullptr) {
                error_code = HOOK_ERROR_CODE_INSUFFICIENT_MEMORY;
                return false;
            }
            if (!Interrupt_Inspect_deserialize_request(
                    *dest.value.state, *encoded_hook, error_code)) {
                error_code =
                    HOOK_ERROR_CODE_SUBSTITUTE_STATE_INSPECT_IS_MALFORMED;
                return false;
            }
            break;
        case ChangeRunningState: {
            uint8_t newRunState = **encoded_hook;
            if (newRunState != WARDUINOrun && newRunState != WARDUINOpause) {
                error_code = HOOK_ERROR_CODE_UNSUPPORTED_RUNNING_STATE;
                return false;
            }
            dest.value.runState = (RunningState)newRunState;
            break;
        }
        case EventInspect:
        case EventRemove:
            break;
        case EventAdd: {
            Event e{"", ""};
            if (!binary_decode_event(e, *encoded_hook)) {
                error_code =
                    HOOK_ERROR_CODE_SUBSTITUTE_STATE_INSPECT_IS_MALFORMED;

                WARDuino::instance()->debugger->channel->write(
                    "failed deserialize eventadd\n");
                return false;
            }
            EventHook *ev =
                new EventHook();  // TODO perhaps move above to avoid malloc
            ev->payload = e.payload.c_str();
            ev->topic = e.topic.c_str();
            ev->payload_length = strlen(ev->payload);
            dest.value.ev = ev;
            break;
        }
        default:
            printf("HookKind %02X is not supported\n", kind);
            error_code = HOOK_ERROR_CODE_UNEXISTING_HOOK_KIND;
            return false;
    }
    dest.kind = kind;
    return true;
}