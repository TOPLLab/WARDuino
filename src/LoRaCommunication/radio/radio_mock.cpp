#include "radio.h"

#if defined(RADIO_MOCK)

#include <queue>
#include <vector>
#include <cstdio>
#include <cstring>
 
// -------------------------------------------------
//      Mock replacements using message queues
// -------------------------------------------------


// One shared FIFO; TX enqueues, RX dequeues.
static std::queue<std::vector<uint8_t>> queue;
 
extern "C" {
 
uint16_t radio_begin_extern(uint32_t freq, uint32_t bw, uint32_t sf, uint32_t cr, uint32_t sw, uint32_t pwr) {
    printf("MOCK: radio_begin\n");
    return 0;
}
 
uint16_t radio_transmit_bytes_extern(uint8_t* data, size_t length) {
    printf("MOCK: message with %zu bytes:", length);
    for (size_t i = 0; i < length; i++) printf(" %02X", data[i]);
    printf("\n");

    queue.push(std::vector<uint8_t>(data, data + length)); // loopback
    return 0;
}

uint16_t radio_receive_bytes_extern(uint8_t* data, size_t length) {
    if (queue.empty()) return 1; // nothing to receive

    auto& packet = queue.front();
    size_t len = std::min(packet.size(), length);
    memcpy(data, packet.data(), len);
    queue.pop();

    printf("MOCK: transmit %zu bytes\n", len);
    return 0;
}

uint16_t radio_startReceive_extern() {
    return 0; // Not implemented
}
 
}

#endif //RADIO_MOCK
