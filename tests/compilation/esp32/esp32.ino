//
// WARDuino - WebAssembly interpreter for embedded devices.
//
//
#include <WARDuino.h>

#include "Arduino.h"

unsigned char wasm[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x04, 0x05, 0x01, 0x70,
    0x01, 0x01, 0x01, 0x05, 0x03, 0x01, 0x00, 0x01, 0x06, 0x13, 0x03, 0x7f,
    0x01, 0x41, 0x80, 0x10, 0x0b, 0x7f, 0x00, 0x41, 0x80, 0x10, 0x0b, 0x7f,
    0x00, 0x41, 0x80, 0x10, 0x0b, 0x07, 0x25, 0x03, 0x06, 0x6d, 0x65, 0x6d,
    0x6f, 0x72, 0x79, 0x02, 0x00, 0x0a, 0x5f, 0x5f, 0x64, 0x61, 0x74, 0x61,
    0x5f, 0x65, 0x6e, 0x64, 0x03, 0x01, 0x0b, 0x5f, 0x5f, 0x68, 0x65, 0x61,
    0x70, 0x5f, 0x62, 0x61, 0x73, 0x65, 0x03, 0x02};
unsigned int wasm_len = 80;

WARDuino* wac = WARDuino::instance();
Module* m;

#define UART_PIN 3

void startDebuggerStd(void* pvParameter) {
    Channel* sink = new Sink(stdout);
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
