#pragma once
#include "../Debug/runningstate.h"
#include "../Interrupts/interrupt_inspect.h"
#include "../WARDuino/structs.h"
#include "./logical_clock.h"
#include "./schedule.h"

#define HOOK_ERROR_CODE_SUBSTITUTE_VALUE_IS_MALFORMED 31;
#define HOOK_ERROR_CODE_UNEXISTING_HOOK_KIND 32;
#define HOOK_ERROR_CODE_UNEXISTING_SCHEDULE_KIND 33;
#define HOOK_ERROR_CODE_SUBSTITUTE_STATE_INSPECT_IS_MALFORMED 34;
#define HOOK_ERROR_CODE_INSUFFICIENT_MEMORY 35;
#define HOOK_ERROR_CODE_UNSUPPORTED_RUNNING_STATE 36;

// todo refactor parse event
// move error code
#define HOOK_ERROR_CODE_EVENT_ADD_FAIL_PARSE 37;
#define HOOK_ERROR_CODE_EVENT_ADD_EMPTY_TOPIC 38;

typedef struct EventHook {
    const char *topic{};
    const char *payload{};
    unsigned int payload_length{};
} EventHook;

enum HookKind {
    RemoteCall = 0x01,
    ValueSubstitution = 0x02,
    StateInspect = 0x03,
    ChangeRunningState = 0x04,
    ProxyCall = 0x05,

    // event hooks from 0x10 to 0x1f
    EventInspect = 0x10,
    EventRemove = 0x11,  // disallow when hookmoment === HookAfterEventHandled,
                         // maybe also for OnEventHandling?
    EventAdd = 0x12,
};

struct Hook {
    HookKind kind;
    uint32_t id;
    union {
        uint32_t target_fidx;
        StackValue *result{};
        StateToInspect *state;
        RunningState runState;
        EventHook *ev;
    } value;
    Schedule schedule{};
    Hook *nextHook{};
};

/*
 * sorts all the hooks based on the order they will be executed.
 * function assumes that `hooks` are already sorted
 */
Hook *Hooks_add_and_sort(Hook *hooks, Hook *hook_to_add);

bool Hook_isScheduledForNow(LogicalClock ct, Schedule s);

bool Hooks_isHookWaitingForEvent(Hook *sorted_hooks,
                                 const LogicalClock &currentTime);

Hook *Hooks_copyHook(const Hook &hook);

typedef struct HooksRemoveResult {
    Hook *newList{};
    Hook *nextHook{};
} HooksRemoveResult;

/*
 * Removes from a hooks linked list `first_hook` the completed hook
 * `hook_completed` and returns the updated linked list
 */
Hook *Hooks_remove_completed_hook(Hook *hooks);

void Hooks_free_hook(Hook *hook);

void Hooks_free_hooks(Hook *hook);

bool Hooks_deserialize_hook(Hook &dest, uint32_t id, uint8_t **encoded_hook,
                            uint8_t &error_code);

bool Hooks_deserialize_schedule(Schedule &dest, uint8_t **encoded_schedule,
                                uint8_t &error_code);
