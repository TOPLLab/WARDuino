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
            buff[buff_len++] = (int8_t)Serial.read();
        }
        if (buff_len) {
            wac.handleInterrupt(buff_len, buff);
        }
    }
    handelingInterrupt = false;
}

void setup() {
    Serial.begin(115200);
    attachInterrupt(D1, handleInput, CHANGE);
}

void loop() {
    Module* m = wac.load_module(hello_world_wasm, hello_world_wasm_len, {});
    printf("START\n\n");
    for (size_t i = 0; i < 10; i++) {
        wac.run_module(m);
        printf("%z\n", i);
    }
    printf("DONE\n\n");
}