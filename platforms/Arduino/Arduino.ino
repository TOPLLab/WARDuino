//
// WARDuino - WebAssembly interpreter for embedded devices.
//
//
//#include <cstdio>

#include <WARDuino.h>

#include "Arduino.h"
#include "upload.h"

// unsigned int wasm_len = _tmp_warduino_upload_wasm_len;
// unsigned char* wasm = _tmp_warduino_upload_wasm;

unsigned int wasm_len = upload_wasm_len;
unsigned char* wasm = upload_wasm;

WARDuino* wac = WARDuino::instance();
Module* m;

#define UART_PIN 3

void startDebuggerStd(void* pvParameter) {
    Channel* sink = new OutChannel(stdout);
    wac->debugger->setChannel(sink);
    sink->open();

    uint8_t buffer[1024] = {0};
    while (true) {
        yield();

        while (Serial.available()) {
            size_t buff_len = 0;
            while (Serial.available()) {
                buffer[buff_len++] = (int8_t)Serial.read();
            }
            if (buff_len) {
                buffer[buff_len] = '\0';
                wac->handleInterrupt(buff_len, buffer);
            }
        }
    }
}

void setup(void) {
    Serial.begin(115200);
    //    attachInterrupt(UART_PIN, handleInput, CHANGE);

    Serial.println(ESP.getFreeHeap());
    Serial.println("Total heap:");
    Serial.println(ESP.getHeapSize());
    Serial.println("\nFree heap:");
    Serial.println(ESP.getFreeHeap());
    Serial.println("\nTotal PSRAM:");
    Serial.println(ESP.getPsramSize());
    Serial.println("\nFree PSRAM: ");
    Serial.println(ESP.getFreePsram());
}

void loop() {
    disableCore0WDT();
    m = wac->load_module(wasm, wasm_len, {});

    printf("LOADED \n\n");
    uint8_t command[] = {'0', '3', '\n'};
    wac->handleInterrupt(3, command);
    xTaskCreate(startDebuggerStd, "Debug Thread", 5000, NULL, 1, NULL);
    printf("START\n\n");

    Serial.println("\nFree heap:");
    Serial.println(ESP.getFreeHeap());

    wac->run_module(m);
    printf("END\n\n");
    wac->unload_module(m);
}
