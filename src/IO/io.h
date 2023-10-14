#include <cstdint>
#include <vector>

struct PinState {
    uint8_t pin;
    bool output;
    uint8_t value;
};

std::vector<PinState*> get_io_state();
