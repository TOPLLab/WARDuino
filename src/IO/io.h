#include <cstdint>
#include <vector>

struct PinState {
    std::string pin;
    bool output;
    uint8_t value;
};

std::vector<PinState*> get_io_state(Module *m);
