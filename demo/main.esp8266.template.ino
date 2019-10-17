#include "Arduino.h"
#include "WARDuino.h"
#include <Ticker.h>
WARDuino wac;

#define  DBGTIMEOUT 500000


volatile bool handlingInterrupt = false;
uint8_t buff[100] = {0};
uint8_t buff_len = 0;

void ICACHE_RAM_ATTR dbgCheck(){
    noInterrupts();
    bool oldHandeling = handlingInterrupt;
    handlingInterrupt = true;
    interrupts();
    if (oldHandeling) return;

    while (Serial.available()) {
        size_t buff_len = 0;
        while (Serial.available()) {
            buff[buff_len++] = (int8_t) Serial.read();
            if(buff_len >= 99) break;
        }
        if (buff_len) {
            wac.handleInterrupt(buff_len, buff);
        }
    }
    handlingInterrupt = false;

    // restart timer
    timer1_write(DBGTIMEOUT);
}

void setup()
{
    Serial.begin(115200);

    // Set timer
    timer1_attachInterrupt(dbgCheck);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
    timer1_write(DBGTIMEOUT);
}

void loop() {
    Module* m = wac.load_module(
            {{src}}, {{src}}_len,
            {.disable_memory_bounds = false, .mangle_table_index=false, .dlsym_trim_underscore=true});
    wac.run_module(m);
}
