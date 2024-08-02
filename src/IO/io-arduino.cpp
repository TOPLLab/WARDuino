#include <cstdint>
#include <vector>

#include "Arduino.h"
#include "io.h"

std::vector<PinState *> get_io_state() {
    std::vector<PinState *> ioState;
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
        uint32_t bit_mask = digitalPinToBitMask(i);
        auto *state = new PinState();
        ioState.push_back(state);
        state->pin = i;
        uint8_t port = digitalPinToPort(i);
        if (*portModeRegister(port) & bit_mask) {  // DDR
            state->output = true;
            state->value = (*portOutputRegister(port) & bit_mask) > 0;
        } else {
            state->output = false;
            state->value = (*portInputRegister(port) & bit_mask) > 0;
        }
    }
    return ioState;
}
