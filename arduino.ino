#include "Arduino.h"
#include "WARDuino.h"

#define D1 5

#include "wa_sources/hello_world.c"
WARDuino wac;

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
            buff[buff_len++]=(int8_t)Serial.read();
        }
        if (buff_len) {
            wac.handleInterrupt(buff_len, buff);
        }
    }
    handelingInterrupt = false;
}

void setup() {
    Serial.begin(115200);
    wdt_reset();
    attachInterrupt(D1, handleInput, CHANGE);
}

void loop() {
    Serial.println(ESP.getFreeHeap());
    int i = wac.run_module(hello_world_wasm, hello_world_wasm_len);
    Serial.println(i, HEX);
    Serial.println("DONE");

    while (true) {
        delay(5000);
    }
}