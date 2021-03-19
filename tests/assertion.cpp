#include "assertion.h"
#include <cstdlib>

Result *makeEmptyResult() {
    auto *result = (Result *) malloc(sizeof(Result));
    result->type = EMPTY;
    result->value = nullptr;
    return result;
}

Result *makeValueResult(Value *val) {
    auto *result = (Result *) malloc(sizeof(Result));
    result->type = VAL;
    result->value = val;
    return result;
}

Action *makeInvokeAction(char *name, Value *expr) {
    auto *action = (Action *) malloc(sizeof(Action));
    action->type = INVOKE;
    action->name = name;
    action->expr = expr;
    return action;
}

Assertion *makeAssertionReturn(Action *action, Result *result) {
    auto *assertion = (Assertion *) malloc(sizeof(Assertion));
    assertion->type = RETURN;
    assertion->action = action;
    assertion->result = result;
    return assertion;
}

Assertion *makeAssertionExhaustion(Action *action) {
    auto *assertion = (Assertion *) malloc(sizeof(Assertion));
    assertion->type = EXHAUSTION;
    assertion->action = action;
    assertion->result = makeEmptyResult();
    return assertion;
}

Value *makeUI64(uint64_t num) {
    auto *value = (Value *) malloc(sizeof(Value));
    value->type = UI64;
    value->uint64 = num;
    return value;
}

Value *makeI64(int64_t num) {
    auto *value = (Value *) malloc(sizeof(Value));
    value->type = I64V;
    value->int64 = num;
    return value;
}

Value *makeI32(int32_t num) {
    auto *value = (Value *) malloc(sizeof(Value));
    value->type = I32V;
    value->int32 = num;
    return value;
}

Value *makeF32(float num) {
    auto *value = (Value *) malloc(sizeof(Value));
    value->type = F32V;
    value->f32 = num;
    return value;
}

Value *makeF64(double num) {
    auto *value = (Value *) malloc(sizeof(Value));
    value->type = F64V;
    value->f64 = num;
    return value;
}

Value *makeSTR(char *text) {
    auto *value = (Value *) malloc(sizeof(Value));
    value->type = STR;
    value->str = text;
    return value;
}
