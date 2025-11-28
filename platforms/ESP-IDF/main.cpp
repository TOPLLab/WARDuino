//
// WARDuino - WebAssembly interpreter for embedded devices.
//
//
#include <stdio.h>
#include <vector>
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

struct ModuleInfo {
    unsigned char *wasm;
    unsigned int wasm_len;
    const char *name;
};

ModuleInfo modules[] = {
    {wasm, wasm_len, "main"},
};
const size_t module_count = sizeof(modules) / sizeof(modules[0]);

extern "C" {
extern void app_main(void);
}

WARDuino* wac = WARDuino::instance();
std::vector<Module*> loaded_modules;

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
    // Load all modules
    for (size_t i = 0; i < module_count; i++) {
        Module* mod = wac->load_module(modules[i].wasm, modules[i].wasm_len,
                                       modules[i].name,
                                       {.disable_memory_bounds = false,
                                        .mangle_table_index = false,
                                        .dlsym_trim_underscore = false,
                                        .return_exception = true});
        
        if (mod) {
            loaded_modules.push_back(mod);
            printf("  ✓ Loaded %s (%u bytes)\n", modules[i].name,
                   modules[i].wasm_len);
        } else {
            printf("  ✗ Failed to load %s\n", modules[i].name);
        }
    }
    
    xTaskCreate(startDebuggerStd, "Debug Thread", 5000, NULL,
                10 /**tskIDLE_PRIORITY*/, NULL);
    
    printf("START\n\n");
    
    if (!loaded_modules.empty()) {
        Module* m = loaded_modules.back();
        wac->run_module(m);
    }
    
    printf("END\n\n");
    
    for (auto mod : loaded_modules) {
        wac->unload_module(mod);
    }
    loaded_modules.clear();
}