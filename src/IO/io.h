#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct IOStateElement;

#include "../WARDuino.h"

struct IOStateElement {
    std::string key;
    bool output;
    int value;
};

std::vector<IOStateElement *> get_io_state(Module *m);
