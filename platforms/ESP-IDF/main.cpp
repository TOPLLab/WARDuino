//
// WARDuino - WebAssembly interpreter for embedded devices.
//
//
#include <stdio.h>

#include "../../src/WARDuino.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "upload.h"

volatile bool handelingInterrupt = false;

unsigned int wasm_len = upload_wasm_len;
unsigned char* wasm = upload_wasm;

extern "C" {
extern void app_main(void);
}

WARDuino* wac = WARDuino::instance();
Module* m;

void startDebuggerStd(void* pvParameter) {
    Channel* duplex = new Duplex(stdin, stdout);
    wac->debugger->setChannel(duplex);
    duplex->open();

    int valread;
    uint8_t buffer[1024] = {0};
    while (true) {
        taskYIELD();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        while ((valread = duplex->read(buffer, 1024)) != -1) {
            wac->handleInterrupt(valread, buffer);
        }
    }
}

void app_main(void) {
    m = wac->load_module(wasm, wasm_len, {});
    //    uint8_t command[] = {'0', '3', '\n'};
    //    wac->handleInterrupt(3, command);
    xTaskCreate(startDebuggerStd, "Debug Thread", 5000, NULL,
                10 /**tskIDLE_PRIORITY*/, NULL);
    printf("START\n\n");
    wac->run_module(m);
    printf("END\n\n");
    wac->unload_module(m);
}
