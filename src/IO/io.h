#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct PinState;

#include "../WARDuino.h"

struct PinState {
    std::string key;
    bool output;
    int value;
};

std::vector<PinState*> get_io_state(Module *m);
