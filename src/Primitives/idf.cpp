#ifndef ARDUINO

/**
 * This file lists the primitives of the language and stores them in the
 * primitives
 *
 * Adding a primitive:
 *  1) Bump up the NUM_PRIMITIVES constant
 *  2) Define <primitive>_type
 *  3) Define a function void primitive(Module* m)
 *  4) Extend the install_primitives function
 *
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/time.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "driver/gpio.h"
#include "primitives.h"

#define NUM_GLOBALS 0
#define ALL_GLOBALS NUM_GLOBALS

int global_index = 0;

#define def_glob(name, type, mut, init_value)             \
    StackValue name##_sv{.value_type = type, init_value}; \
    Global name = {                                       \
        .mutability = mut, .import_field = #name, .value = &name##_sv};

#define install_global(global_name)                        \
    {                                                      \
        dbg_info("installing global: %s\n", #global_name); \
        if (global_index < ALL_GLOBALS) {                  \
            globals[global_index++] = (global_name);       \
        } else {                                           \
            FATAL("global_index out of bounds");           \
        }                                                  \
    }

// The globals table
Global globals[ALL_GLOBALS];

def_prim(chip_delay, oneToNoneU32) {
    vTaskDelay(warg0.uint32 / portTICK_PERIOD_MS);
    pop_args(1);
    return true;
}

def_prim(chip_pin_mode, twoToNoneU32) {
    gpio_set_direction(gpio_num_t(arg1.uint32), gpio_mode_t(warg0.uint32));
    pop_args(2);
    return true;
}

def_prim(chip_digital_write, twoToNoneU32) {
    gpio_set_level(gpio_num_t(arg1.uint32), gpio_mode_t(warg0.uint32));
    pop_args(2);
    return true;
}

def_prim(chip_digital_read, oneToOneU32) {
    gpio_num_t pin = gpio_num_t(warg0.uint32);
    uint8_t res = (uint8_t)gpio_get_level(pin);
    pop_args(1);
    pushUInt32(res);
    return true;
}

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives(Interpreter *interpreter) {
    dbg_info("INSTALLING PRIMITIVES\n");
    install_primitive(chip_delay);
    install_primitive(chip_pin_mode);
    install_primitive(chip_digital_write);
    install_primitive(chip_digital_read);
}

Memory external_mem = {0, 0, 0, nullptr};

bool resolve_external_memory(char *symbol, Memory **val) {
    if (!strcmp(symbol, "memory")) {
        if (external_mem.bytes == nullptr) {
            external_mem.initial = 256;
            external_mem.maximum = 256;
            external_mem.pages = 256;
            external_mem.bytes = (uint8_t *)acalloc(
                external_mem.pages * PAGE_SIZE, sizeof(uint32_t),
                "Module->memory.bytes primitive");
        }
        *val = &external_mem;
        return true;
    }

    FATAL("Could not find memory %s \n", symbol);
    return false;
}

bool resolve_external_global(char *symbol, Global **val) {
    debug("Resolve external global for %s  \n", symbol);

    for (auto &global : globals) {
        if (!strcmp(symbol, global.import_field)) {
            *val = &global;
            return true;
        }
    }
    FATAL("Could not find global %s \n", symbol);
    return false;
}

#endif  // ARDUINO
