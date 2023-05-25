#pragma once
#include "callstackbuilder.h"

CallstackBuilder::CallstackBuilder(Module* wasm_module) : m{wasm_module} {}

void CallstackBuilder::pushBlock(Block* block, int sp) {
    // copy of push_block instructions.cpp
    m->csp += 1;
    m->callstack[m->csp].block = block;
    m->callstack[m->csp].sp = sp;
    m->callstack[m->csp].fp = m->fp;
    m->callstack[m->csp].ra_ptr = m->pc_ptr;
}

void CallstackBuilder::pushFunctionCall(uint32_t fidx) {
    // copy of setup_call from instructions.cpp
    Block* func = &m->functions[fidx];
    Type* type = func->type;

    // Push current frame on the call stack
    this->pushBlock(func, m->sp - type->param_count);

    // Push locals (dropping extras)
    m->fp = m->sp - ((int)type->param_count) + 1;

    // Push function locals
    for (uint32_t lidx = 0; lidx < func->local_count; lidx++) {
        m->sp += 1;
        m->stack[m->sp].value_type = func->local_value_type[lidx];
        m->stack[m->sp].value.uint64 = 0;  // Initialize whole union to 0
    }

    // Set program counter to start of function
    m->pc_ptr = func->start_ptr;
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
    this->pushBlock(guard, m->sp);
}