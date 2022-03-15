//
// WARDuino - WebAssembly interpreter for embedded devices.
//
//
//#include <cstdio>

#include <WARDuino.h>

#include "Arduino.h"
#include "upload.h"


unsigned int  wasm_len = _tmp_warduino_upload_wasm_len;
unsigned char* wasm = _tmp_warduino_upload_wasm;

WARDuino wac;
Module* m;

#define UART_PIN 3

void startDebuggerStd(void* pvParameter) {
    int valread;
    uint8_t buffer[1024] = {0};
    wac.debugger->socket = fileno(stdout);
    write(fileno(stdout), "Got a message ... \n", 19);
    while (true) {
        // taskYIELD();
        // vTaskDelay(100 / portTICK_PERIOD_MS);
        yield();

        while (Serial.available()) {
            size_t buff_len = 0;
            while (Serial.available()) {
                buffer[buff_len++] = (int8_t)Serial.read();
            }
            if (buff_len) {
                write(fileno(stdout), "Reading message ..... \n", 19);
                fflush(stdout);
                wac.handleInterrupt(valread - 1, buffer);
                write(fileno(stdout), buffer, valread);
                fflush(stdout);
            }
        }
    }
}

void setup(void) {
    Serial.begin(115200);
    //    attachInterrupt(UART_PIN, handleInput, CHANGE);
}

void loop() {
    disableCore0WDT();
    m = wac.load_module(wasm, wasm_len, {});
    xTaskCreate(startDebuggerStd, "Debug Thread", 5000, NULL, 1, NULL);
    printf("START\n\n");
    wac.run_module(m);
    printf("END\n\n");
    wac.unload_module(m);
}
