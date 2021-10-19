#include <Ticker.h>

#include "Arduino.h"
#include "WARDuino.h"
WARDuino wac;

#define DBGTIMEOUT 500000

hw_timer_t* timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool handlingInterrupt = false;
uint8_t buff[100] = {0};
uint8_t buff_len = 0;

void IRAM_ATTR dbgCheck() {
    portENTER_CRITICAL_ISR(&timerMux);
    bool oldHandeling = handlingInterrupt;
    handlingInterrupt = true;
    portEXIT_CRITICAL_ISR(&timerMux);
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
    portENTER_CRITICAL_ISR(&timerMux);
    handlingInterrupt = false;
    portEXIT_CRITICAL_ISR(&timerMux);
}

void setup() {
    Serial.begin(115200);

    // Set timer
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &dbgCheck, true);
    timerAlarmWrite(timer, DBGTIMEOUT, true);
    timerAlarmEnable(timer);
}

void loop() {
    Module* m = wac.load_module({{src}},
                                {
                                    { src }
                                } _len,
                                {.disable_memory_bounds = false,
                                 .mangle_table_index = false,
                                 .dlsym_trim_underscore = true});
    wac.run_module(m);
}
