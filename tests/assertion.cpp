#include "assertion.h"
#include <stdio.h>
#include <stdlib.h>

Result *makeEmptyResult() {
    Result *result = (Result *) malloc(sizeof(Result));
    result->type = EMPTY;
    result->value = nullptr;
    return result;
}

Result *makeValueResult(Value *val) {
    Result *result = (Result *) malloc(sizeof(Result));
    result->type = VAL;
    result->value = val;
    return result;
}

Action *makeInvokeAction(char *name, Value *expr) {
    Action *action = (Action *) malloc(sizeof(Action));
    action->type = INVOKE;
    action->name = name;
    action->expr = expr;
    return action;
}

Assertion *makeAssertionReturn(Action *action, Result *result) {
    Assertion *assertion = (Assertion *) malloc(sizeof(Assertion));
    assertion->type = RETURN;
    assertion->action = action;
    assertion->result = result;
    return assertion;
}

Assertion *makeAssertionExhaustion(Action *action) {
    Assertion *assertion = (Assertion *) malloc(sizeof(Assertion));
    assertion->type = EXHAUSTION;
    assertion->action = action;
    assertion->result = makeEmptyResult();
    return assertion;
}

Value *makeUI64(uint64_t num) {
    Value *value = (Value *) malloc(sizeof(Value));
    value->type = UI64;
    value->uint64 = num;
    return value;
}

Value *makeI64(int64_t num) {
    Value *value = (Value *) malloc(sizeof(Value));
    value->type = I64V;
    value->int64 = num;
    return value;
}

Value *makeI32(int32_t num) {
    Value *value = (Value *) malloc(sizeof(Value));
    value->type = I32V;
    value->int32 = num;
    return value;
}
