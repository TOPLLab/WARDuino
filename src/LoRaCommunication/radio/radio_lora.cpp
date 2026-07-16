#include "radio.h"

#if defined(RADIO_LORA)

#include <RadioLib.h>

// -------------------------------------------------
//    RadioLib procedures for lora communication
// -------------------------------------------------

// SX1262( NSS, DIO1, RESET, BUSY )
Module radio_module(5, 4, 3, 6);
SX1262 radio(&radio_module);

extern "C" {
    uint16_t radio_begin_extern(uint32_t freq, uint32_t bw, uint32_t sf, uint32_t cr, uint32_t sw, uint32_t pwr) {
        return radio.begin(freq, bw, sf, cr, sw, pwr);
    }
    uint16_t radio_transmit_bytes_extern(uint8_t* data, size_t length) {
        return radio.transmit(data, length);
    }
    uint16_t radio_receive_bytes_extern(uint8_t* data, size_t length) {
        return radio.receive(data, length);
    }
    uint16_t radio_startReceive_extern() {
        return radio.startReceive();
    }
    uint16_t radio_transmit_extern(String data) {
        return radio.transmit(data);
    }
    uint16_t radio_receive_extern(String data, uint32_t timeout) {
        return radio.receive(data, timeout);
    }
}

#endif //RADIO_LORA