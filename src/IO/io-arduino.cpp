#include "io.h"
#include "../WARDuino.h"
#include "Arduino.h"

#include <cstdint>
#include <vector>

std::vector<PinState*> get_io_state(Module *m) {
    std::vector<PinState*> ioState;
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
        uint32_t bit_mask = digitalPinToBitMask(i);
        auto *state = new PinState();
        ioState.push_back(state);
        state->pin = i;
        uint8_t port = digitalPinToPort(i);
        if (*portModeRegister(port) & bit_mask) { // DDR
            state->output = true;
            state->value = (*portOutputRegister(port) & bit_mask) > 0;
        } else {
            state->output = false;
            state->value = (*portInputRegister(port) & bit_mask) > 0;
        }

        if (m->io_override.find(i) != m->io_override.end()) {
            state->value = m->io_override[i];
        }
    }
    return ioState;
}
