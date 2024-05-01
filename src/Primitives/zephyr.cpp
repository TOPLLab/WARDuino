#include <sys/_stdint.h>
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
#include <sys/time.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/devicetree/gpio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "primitives.h"

#define NUM_PRIMITIVES 0
#define NUM_PRIMITIVES_ARDUINO 10

#define ALL_PRIMITIVES (NUM_PRIMITIVES + NUM_PRIMITIVES_ARDUINO)

// Global index for installing primitives
int prim_index = 0;

double sensor_emu = 0;

/*
   Private macros to install a primitive
*/
#define install_primitive(prim_name)                                       \
    {                                                                      \
        dbg_info("installing primitive number: %d  of %d with name: %s\n", \
                 prim_index + 1, ALL_PRIMITIVES, #prim_name);              \
        if (prim_index < ALL_PRIMITIVES) {                                 \
            PrimitiveEntry *p = &primitives[prim_index++];                 \
            p->name = #prim_name;                                          \
            p->f = &(prim_name);                                           \
        } else {                                                           \
            FATAL("prim_index out of bounds");                             \
        }                                                                  \
    }

#define def_prim(function_name, type) \
    Type function_name##_type = type; \
    bool function_name(Module *m)

// TODO: use fp
#define pop_args(n) m->sp -= n
#define get_arg(m, arg) m->stack[(m)->sp - (arg)].value
#define pushUInt32(arg) m->stack[++m->sp].value.uint32 = arg
#define pushInt32(arg) m->stack[++m->sp].value.int32 = arg
#define pushUInt64(arg)                 \
    m->stack[++m->sp].value_type = I64; \
    m->stack[m->sp].value.uint64 = arg
#define arg0 get_arg(m, 0)
#define arg1 get_arg(m, 1)
#define arg2 get_arg(m, 2)
#define arg3 get_arg(m, 3)
#define arg4 get_arg(m, 4)
#define arg5 get_arg(m, 5)
#define arg6 get_arg(m, 6)
#define arg7 get_arg(m, 7)
#define arg8 get_arg(m, 8)
#define arg9 get_arg(m, 9)

// The primitive table
PrimitiveEntry primitives[ALL_PRIMITIVES];

//
uint32_t param_arr_len0[0] = {};
uint32_t param_I32_arr_len1[1] = {I32};
uint32_t param_I32_arr_len2[2] = {I32, I32};
uint32_t param_I32_arr_len3[3] = {I32, I32, I32};
uint32_t param_I32_arr_len4[4] = {I32, I32, I32, I32};
uint32_t param_I32_arr_len10[10] = {I32, I32, I32, I32, I32,
                                    I32, I32, I32, I32, I32};

uint32_t param_I64_arr_len1[1] = {I64};

Type oneToNoneU32 = {
    .form = FUNC,
    .param_count = 1,
    .params = param_I32_arr_len1,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x8001 /* 0x800 = no return ; 1 = I32*/
};

Type twoToNoneU32 = {
    .form = FUNC,
    .param_count = 2,
    .params = param_I32_arr_len2,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x80011 /* 0x800 = no return ; 1 = I32; 1 = I32*/
};

Type threeToNoneU32 = {
    .form = FUNC,
    .param_count = 3,
    .params = param_I32_arr_len3,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x800111 /* 0x800 = no return ; 1=I32; 1=I32; 1=I32*/
};

Type fourToNoneU32 = {
    .form = FUNC,
    .param_count = 4,
    .params = param_I32_arr_len4,
    .result_count = 0,
    .results = nullptr,
    .mask =
        0x8001111 /* 0x800 = no return ; 1 = I32; 1 = I32; 1 = I32; 1 = I32*/
};

Type oneToOneU32 = {
    .form = FUNC,
    .param_count = 1,
    .params = param_I32_arr_len1,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x80011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

Type oneToOneI32 = {
    .form = FUNC,
    .param_count = 1,
    .params = param_I32_arr_len1,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x80011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

Type twoToOneU32 = {
    .form = FUNC,
    .param_count = 2,
    .params = param_I32_arr_len2,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x81011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

Type threeToOneU32 = {
    .form = FUNC,
    .param_count = 3,
    .params = param_I32_arr_len3,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x810111 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32; 1=I32*/
};

Type fourToOneU32 = {
    .form = FUNC,
    .param_count = 4,
    .params = param_I32_arr_len4,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x8101111 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32; 1=I32; 1=I32*/
};

Type tenToOneU32 = {
    .form = FUNC,
    .param_count = 10,
    .params = param_I32_arr_len10,
    .result_count = 1,
    .results = param_I32_arr_len1,
    .mask = 0x8101111111111 /* 0x8 1=I32 0=endRet ; 10 params 1=I32*/
};

Type NoneToNoneU32 = {.form = FUNC,
                      .param_count = 0,
                      .params = nullptr,
                      .result_count = 0,
                      .results = nullptr,
                      .mask = 0x80000};

Type NoneToOneU32 = {.form = FUNC,
                     .param_count = 0,
                     .params = nullptr,
                     .result_count = 1,
                     .results = param_I32_arr_len1,
                     .mask = 0x81000};

Type NoneToOneU64 = {.form = FUNC,
                     .param_count = 0,
                     .params = nullptr,
                     .result_count = 1,
                     .results = param_I64_arr_len1,
                     .mask = 0x82000};

def_prim(chip_delay, oneToNoneU32) {
    debug("chip_delay(%u)\n", arg0.uint32);
    k_msleep(arg0.uint32);
    pop_args(1);
    return true;
}

def_prim(chip_delay_us, oneToNoneU32) {
    debug("chip_delay_us(%u)\n", arg0.uint32);
    k_yield();
    k_usleep(arg0.uint32);
    pop_args(1);
    return true;
}

struct gpio_dt_spec specs[] = {DT_FOREACH_PROP_ELEM_SEP(
    DT_PATH(zephyr_user), warduino_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))};

def_prim(chip_pin_mode, twoToNoneU32) {
    debug("chip_pin_mode(%u,%u)\n", arg1.uint32, arg0.uint32);
    gpio_dt_spec pin_spec = specs[arg1.uint32];
    gpio_pin_configure(pin_spec.port, pin_spec.pin,
                       arg0.uint32 == 0 ? GPIO_INPUT : GPIO_OUTPUT);
    pop_args(2);
    return true;
}

def_prim(chip_digital_write, twoToNoneU32) {
    debug("chip_digital_write(%u,%u)\n", arg1.uint32, arg0.uint32);
    gpio_dt_spec pin_spec = specs[arg1.uint32];
    gpio_pin_set_raw(pin_spec.port, pin_spec.pin, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(chip_digital_read, oneToOneU32) {
    debug("chip_digital_read(%u)\n", arg0.uint32);
    gpio_dt_spec pin_spec = specs[arg0.uint32];
    uint8_t res = gpio_pin_get_raw(pin_spec.port, pin_spec.pin);
    pop_args(1);
    pushUInt32(res);
    return true;
}

def_prim(print_int, oneToNoneU32) {
    printf("%u\n", arg0.uint32);
    pop_args(1);
    return true;
}

const struct device *const spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi0));

int write_spi_bytes(unsigned char *data, size_t len) {
    if (!device_is_ready(spi_dev)) {
        printk("%s: device not ready.\n", spi_dev->name);
        return -1;
    }

    struct spi_buf tx_buf = { .buf = data, .len = len };
    struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

    // Now write the buffer using SPI.
    struct spi_config config;
    config.frequency = DT_PROP(DT_NODELABEL(spi0), clock_frequency);
    config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8);
    config.slave = 0;
    config.cs = {};

    return spi_write(spi_dev, &config, &tx_bufs);
}

int write_spi_bytes_16_prim(int times, unsigned int color) {
    unsigned char *data = new unsigned char[times * 2];
    unsigned char colorB = color >> 8;
    for (int x = 0; x < times; x++) {
        data[x * 2] = color;
        data[x * 2 + 1] = colorB;
    }

    int result = write_spi_bytes(data, times * 2);
    delete[] data;
    return result;
}

def_prim(write_spi_byte, oneToNoneU32) {
    debug("write_spi_byte(%d)\n", arg0.uint32);
    unsigned char data = (char) arg0.uint32;
    int result = write_spi_bytes(&data, 1);
    pop_args(1);
    return result == 0;
}

def_prim(write_spi_bytes_16, twoToNoneU32) {
    debug("write_spi_bytes_16(%d, %d)\n", arg1.uint32, arg0.uint32);
    int result = write_spi_bytes_16_prim(arg1.uint32, arg0.uint32);
    pop_args(2);
    return result == 0;
}

def_prim(read_spi_byte, NoneToOneU32) {
    debug("read_spi_byte(%u)\n", arg0.uint32);
    struct spi_config config;

    config.frequency = DT_PROP(DT_NODELABEL(spi0), clock_frequency);
    config.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(9);
    config.slave = 0;
    config.cs = {}; // TODO: Chip select line, maybe useful or can we do it manually not sure

    unsigned char data = 0;
    struct spi_buf tx_buf = { .buf = &data, .len = 1 };
    struct spi_buf_set tx_bufs = { .buffers = &tx_buf, .count = 1 };

    printf("data= %c\n", data);

    int result = spi_read(spi_dev, &config, &tx_bufs);
    printf("result= %d\n", result);
    pushInt32(0);
    return true;
}

def_prim(abort, NoneToNoneU32) {
    sprintf(exception, "Trap: assertion failed.");
    return false;
}

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives() {
    dbg_info("INSTALLING PRIMITIVES\n");
    install_primitive(chip_delay);
    install_primitive(chip_delay_us);
    install_primitive(chip_pin_mode);
    install_primitive(chip_digital_write);
    install_primitive(chip_digital_read);
    install_primitive(print_int);
    install_primitive(write_spi_byte);
    install_primitive(write_spi_bytes_16);
    install_primitive(read_spi_byte);
    install_primitive(abort);
}

//------------------------------------------------------
// resolving the primitives
//------------------------------------------------------
bool resolve_primitive(char *symbol, Primitive *val) {
    debug("Resolve primitives (%d) for %s  \n", ALL_PRIMITIVES, symbol);

    for (auto &primitive : primitives) {
        //        printf("Checking %s = %s  \n", symbol, primitive.name);
        if (!strcmp(symbol, primitive.name)) {
            debug("FOUND PRIMITIVE\n");
            *val = primitive.f;
            return true;
        }
    }
    FATAL("Could not find primitive %s \n", symbol);
    return false;
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

#endif  // ARDUINO
