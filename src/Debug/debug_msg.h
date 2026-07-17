#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "../Interrupts/interrupts.h"

struct DebugMessage {
    uint8_t *data{};
    uint8_t *fullData{};
    size_t len{};
    InterruptTypes interrupt{};
    uint32_t id{};
};

DebugMessage *DebugMessage_new(uint8_t *fullData, size_t size);

void DebugMessage_free(DebugMessage *msg);
