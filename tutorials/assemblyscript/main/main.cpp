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

Module* m;

void startDebuggerStd(void* pvParameter) {
    int valread;
    uint8_t buffer[1024] = {0};
    Channel* duplex = new Duplex(stdin, stdout);
    WARDuino::instance()->debugger->channel = duplex;
    while (true) {
        taskYIELD();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        while ((valread = duplex->read(buffer, 1024)) != -1) {
            WARDuino::instance()->handleInterrupt(valread, buffer);
        }
    }
}

void app_main(void) {
    m = WARDuino::instance()->load_module(src_wasm, src_wasm_len, {});
    xTaskCreate(startDebuggerStd, "Debug Thread", 5000, NULL, 1, NULL);
    printf("START\n\n");
    WARDuino::instance()->run_module(m);
    printf("END\n\n");
    WARDuino::instance()->unload_module(m);
}
