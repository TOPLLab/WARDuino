#include "io.h"
#include "Arduino.h"

#include <cstdint>
#include <vector>

std::vector<PinState*> get_io_state() {
    std::vector<PinState*> ioState;
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
        auto *state = new PinState();
        ioState.push_back(state);
        state->pin = i;
        uint8_t port = digitalPinToPort(i);
        uint8_t output = bitRead(*portModeRegister(port), i % 32); // DDR
        if (output) {
            state->output = true;
            state->value = bitRead(*portOutputRegister(port), i % 32);
        } else {
            state->output = false;
            state->value = bitRead(*portInputRegister(port), i % 32);
        }
    }
    return ioState;
}
