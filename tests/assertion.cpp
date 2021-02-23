#include "assertion.h"

#include <stdio.h>
#include <stdlib.h>

Result* makeValueResult(Value* val) {
    Result* result = (Result*)malloc(sizeof(Result));
    result->type = VAL;
    result->value = val;
    return result;
}

Action* makeInvokeAction(char* name, Value* expr) {
    Action* action = (Action*)malloc(sizeof(Action));
    action->type = INVOKE;
    action->name = name;
    action->expr = expr;
    return action;
}

Assertion* makeAssertionReturn(Action* action, Result* result) {
    Assertion* assertion = (Assertion*)malloc(sizeof(Assertion));
    assertion->type = RETURN;
    assertion->action = action;
    assertion->result = result;
    return assertion;
}

Value* makeUI64(uint64_t num) {
    Value* value = (Value*)malloc(sizeof(Value));
    value->type = UI64;
    value->uint64 = num;
    return value;
}

Value* makeI64(int64_t num) {
    Value* value = (Value*)malloc(sizeof(Value));
    value->type = I64V;
    value->int64 = num;
    return value;
}
