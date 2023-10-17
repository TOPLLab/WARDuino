#ifndef ARDUINO
#include "io.h"
#include "../WARDuino.h"

std::vector<PinState*> get_io_state(Module *m) {
    return {};
}
#endif
