//
// WARDuino - WebAssembly interpreter for embedded devices.
//
//
#include <stdio.h>

#include "../../../../src/WARDuino.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "esp_vfs_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

volatile bool handelingInterrupt = false;

#include "src.wasm.h"

extern "C" {
extern void app_main(void);
}

WARDuino wac;
Module* m;

void startDebuggerStd(void* pvParameter) {
    int valread;
    uint8_t buffer[1024] = {0};
    wac.debugger->socket = fileno(stdout);
    while (true) {
        taskYIELD();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        while ((valread = read(fileno(stdin), buffer, 1024)) != -1) {
            write(fileno(stdout), "got a message ... \n", 19);
            wac.handleInterrupt(valread - 1, buffer);
            write(fileno(stdout), buffer, valread);
            fflush(stdout);
        }
    }
}

void app_main(void) {
    m = wac.load_module(src_wasm, src_wasm_len, {});
    xTaskCreate(startDebuggerStd, "Debug Thread", 5000, NULL, 1, NULL);
    printf("START\n\n");
    wac.run_module(m);
    printf("END\n\n");
    wac.unload_module(m);
}
