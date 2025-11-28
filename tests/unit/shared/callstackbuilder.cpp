#pragma once
#include "callstackbuilder.h"

CallstackBuilder::CallstackBuilder(Module* wasm_module) : m{wasm_module} {}

void CallstackBuilder::pushBlock(Block* block, int sp) {
    // copy of push_block instructions.cpp
    ExecutionContext* ectx = m->warduino->execution_context;
    ectx->csp += 1;
    ectx->callstack[ectx->csp].block = block;
    ectx->callstack[ectx->csp].sp = sp;
    ectx->callstack[ectx->csp].fp = ectx->fp;
    ectx->callstack[ectx->csp].ra_ptr = ectx->pc_ptr;
}

void CallstackBuilder::pushFunctionCall(uint32_t fidx) {
    ExecutionContext* ectx = m->warduino->execution_context;
    // copy of setup_call from instructions.cpp
    Block* func = &m->functions[fidx];
    Type* type = func->type;

    // Push current frame on the call stack
    this->pushBlock(func, ectx->sp - type->param_count);

    // Push locals (dropping extras)
    ectx->fp = ectx->sp - ((int)type->param_count) + 1;

    // Push function locals
    for (uint32_t lidx = 0; lidx < func->local_count; lidx++) {
        ectx->sp += 1;
        ectx->stack[ectx->sp].value_type = func->local_value_type[lidx];
        ectx->stack[ectx->sp].value.uint64 = 0;  // Initialize whole union to 0
    }

    // Set program counter to start of function
    ectx->pc_ptr = func->start_ptr;
}

void CallstackBuilder::pushGuard(uint8_t guard_type) {
    auto* guard = (Block*)malloc(sizeof(struct Block));
    guard->block_type = guard_type;
    guard->type = nullptr;
    guard->local_value_type = nullptr;
    guard->start_ptr = nullptr;
    guard->end_ptr = nullptr;
    guard->else_ptr = nullptr;
    guard->export_name = nullptr;
    guard->import_field = nullptr;
    guard->import_module = nullptr;
    guard->func_ptr = nullptr;
    this->pushBlock(guard, m->warduino->execution_context->sp);
}