#include "circuit.h"
#include "radio/radio.h"

void sendSecureMessage(const char* message, uint16_t destNodeID) {
    Circuit* circuit = buildCircuit(destNodeID);
    if (circuit != nullptr) {
        while (circuit->state != CircuitState::READY) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        sendDataForwards(circuit, message, strlen(message) + 1);
    }
}