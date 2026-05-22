#include <stdlib.h>

extern "C" {
    uint16_t radio_begin_extern(uint32_t freq, uint32_t bw, uint32_t sf, uint32_t cr, uint32_t sw, uint32_t pwr);
    uint16_t radio_transmit_bytes_extern(uint8_t* data, size_t len);
    //uint16_t radio_transmit_extern(String data);
    uint16_t radio_startReceive_extern();
    uint16_t radio_receive_bytes_extern(uint8_t* data, size_t maxLen);
    //uint16_t radio_receive_extern(String data, uint32_t timeout);
}