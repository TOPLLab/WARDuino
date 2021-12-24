#include <Ticker.h>

#include "Arduino.h"
#include "WARDuino.h"

unsigned char hello_wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x04, 0x05, 0x01, 0x70,
    0x01, 0x01, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x06, 0x13, 0x03, 0x7f,
    0x01, 0x41, 0x80, 0x10, 0x0b, 0x7f, 0x00, 0x41, 0x80, 0x10, 0x0b, 0x7f,
    0x00, 0x41, 0x80, 0x10, 0x0b, 0x07, 0x25, 0x03, 0x06, 0x6d, 0x65, 0x6d,
    0x6f, 0x72, 0x79, 0x02, 0x00, 0x0a, 0x5f, 0x5f, 0x64, 0x61, 0x74, 0x61,
    0x5f, 0x65, 0x6e, 0x64, 0x03, 0x01, 0x0b, 0x5f, 0x5f, 0x68, 0x65, 0x61,
    0x70, 0x5f, 0x62, 0x61, 0x73, 0x65, 0x03, 0x02};
unsigned int hello_wasm_len = 80;

WARDuino wac;

Module* m;

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

    m = wac.load_module(hello_wasm, hello_wasm_len, {});
}

void loop() { wac.run_module(m); }
