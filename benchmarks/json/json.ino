#include <WARDuino.h>

#include "Arduino.h"
#include "bench.h"

WARDuino* wac = WARDuino::instance();

#define D1 5

volatile bool handelingInterrupt = false;
uint8_t buff[100] = {0};
uint8_t buff_len = 0;

void ICACHE_RAM_ATTR handleInput() {
    if (handelingInterrupt) return;
    handelingInterrupt = true;
    interrupts();

    while (Serial.available()) {
        size_t buff_len = 0;
        while (Serial.available()) {
            buff[buff_len++] = (int8_t)Serial.read();
        }
        if (buff_len) {
            wac->handleInterrupt(buff_len, buff);
        }
    }
    handelingInterrupt = false;
}

void setup() {
    Serial.begin(115200);
    attachInterrupt(D1, handleInput, CHANGE);

    Serial.println("START");
    Serial.flush();

    // only execute ones
    Module *m = wac->load_module(bench_wasm, bench_wasm_len, {});
    wac->run_module(m);
    wac->unload_module(m);

    Serial.println("END");
    Serial.flush();

}

void loop() {
}
