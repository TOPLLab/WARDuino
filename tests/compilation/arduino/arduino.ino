#include <Ticker.h>

#include "Arduino.h"
#include "WARDuino.h"
WARDuino wac;

#define DBGTIMEOUT 500000

unsigned char hello_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x04, 0x05, 0x01, 0x70,
    0x01, 0x01, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x06, 0x13, 0x03, 0x7f,
    0x01, 0x41, 0x80, 0x10, 0x0b, 0x7f, 0x00, 0x41, 0x80, 0x10, 0x0b, 0x7f,
    0x00, 0x41, 0x80, 0x10, 0x0b, 0x07, 0x25, 0x03, 0x06, 0x6d, 0x65, 0x6d,
    0x6f, 0x72, 0x79, 0x02, 0x00, 0x0a, 0x5f, 0x5f, 0x64, 0x61, 0x74, 0x61,
    0x5f, 0x65, 0x6e, 0x64, 0x03, 0x01, 0x0b, 0x5f, 0x5f, 0x68, 0x65, 0x61,
    0x70, 0x5f, 0x62, 0x61, 0x73, 0x65, 0x03, 0x02};
unsigned int hello_wasm_len = 80;

volatile bool handlingInterrupt = false;
uint8_t buff[100] = {0};
uint8_t buff_len = 0;

void ICACHE_RAM_ATTR dbgCheck() {
    noInterrupts();
    bool oldHandeling = handlingInterrupt;
    handlingInterrupt = true;
    interrupts();
    if (oldHandeling) return;

    while (Serial.available()) {
        size_t buff_len = 0;
        while (Serial.available()) {
            buff[buff_len++] = (int8_t)Serial.read();
            if (buff_len >= 99) break;
        }
        if (buff_len) {
            wac.handleInterrupt(buff_len, buff);
        }
    }
    handlingInterrupt = false;

    // restart timer
    timer1_write(DBGTIMEOUT);
}

void setup() {
    Serial.begin(115200);

    // Set timer
    timer1_attachInterrupt(dbgCheck);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
    timer1_write(DBGTIMEOUT);
}

void loop() {
    Module* m = wac.load_module(hello_wasm, hello_wasm_len,
                                {.disable_memory_bounds = false,
                                 .mangle_table_index = false,
                                 .dlsym_trim_underscore = true});
    wac.run_module(m);
    wac.unload_module(m);
}
