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
#include "primitives.h"

#include "debug.h"
#include "mem.h"

#ifdef ARDUINO
#include "Arduino.h"

#define delay_us(ms) delayMicroseconds(ms)
#include <SPI.h>
SPIClass *spi = new SPIClass();

//Hardeware SPI
void write_spi_byte(unsigned char c){
  spi->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  spi->transfer(c);
  spi->endTransaction();
}

void write_spi_bytes_16_prim(int times, uint32_t color) {
    unsigned char colorB = color >> 8;
    spi->beginTransaction(SPISettings(200000000, MSBFIRST, SPI_MODE0));
    for (int x=0; x < times; x++) {
        spi->transfer(colorB);
          spi->transfer(color);
    }
      spi->endTransaction();
}

#else

#include <chrono>
#include <thread>

#endif

#include <stdio.h>
#include <string.h>

#define NUM_PRIMITIVES 0
#ifdef ARDUINO
#define NUM_PRIMITIVES_ARDUINO 9
#else
#define NUM_PRIMITIVES_ARDUINO 8
#endif

#define ALL_PRIMITIVES (NUM_PRIMITIVES + NUM_PRIMITIVES_ARDUINO)

// Global index for installing primitives
int prim_index = 0;

/*
    Private macros to install a primitive
*/
#define install_primitive(prim_name)                                       \
    {                                                                      \
        dbg_info("installing primitive number: %d  of %d with name: %s\n", \
                 prim_index+1, ALL_PRIMITIVES, #prim_name);                  \
        if (prim_index < ALL_PRIMITIVES) {                                 \
            PrimitiveEntry* p = &primitives[prim_index++];                 \
            p->name = #prim_name;                                          \
            p->f = &(prim_name);                                             \
        } else {                                                           \
            FATAL("pim_index out of bounds");                              \
        }                                                                  \
    }

#define def_prim(function_name, type) \
    Type function_name##_type = type; \
    bool function_name(Module* m)

// TODO: use fp
#define pop_args(n) m->sp -= n
#define get_arg(m, arg) m->stack[(m)->sp - (arg)].value
#define pushInt32(arg) m->stack[m->sp].value.uint32 = arg
#define arg0 get_arg(m, 0)
#define arg1 get_arg(m, 1)
#define arg2 get_arg(m, 2)
#define arg3 get_arg(m, 2)
#define arg4 get_arg(m, 2)

// The primitive table
PrimitiveEntry primitives[ALL_PRIMITIVES];

//------------------------------------------------------
// Primitive Blink
//------------------------------------------------------
Type nullType;

//
uint32_t param_arr_len0[0] = {};
uint32_t param_I32_arr_len1[1] = {I32};
uint32_t param_I32_arr_len2[2] = {I32, I32};

Type oneToNoneU32 = {
        .form =  FUNC,
        .param_count =  1,
        .params =  param_I32_arr_len1,
        .result_count =  0,
        .results =  nullptr,
        .mask =  0x8001 /* 0x800 = no return ; 1 = I32*/
};

Type twoToNoneU32 = {
        .form =  FUNC,
        .param_count =  2,
        .params =  param_I32_arr_len2,
        .result_count =  0,
        .results =  nullptr,
        .mask =  0x80011 /* 0x800 = no return ; 1 = I32; 1 = I32*/
};


Type oneToOneU32 = {
        .form =  FUNC,
        .param_count =  1,
        .params =  param_I32_arr_len1,
        .result_count =  1,
        .results =  param_I32_arr_len1,
        .mask =  0x80011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

Type NoneToNoneU32 = {
        .form =  FUNC,
        .param_count =  0,
        .params =  nullptr,
        .result_count =  0,
        .results =  nullptr,
        .mask =  0x80000
};


//------------------------------------------------------
// Arduino Specific Functions
//------------------------------------------------------
#ifdef ARDUINO

def_prim(assert_int, oneToNoneU32) {
    uint8_t boolean = arg0.uint32;
    pop_args(1);
    return (bool) boolean;
}


//warning: undefined symbol: chip_pin_mode
def_prim(chip_pin_mode, twoToNoneU32) {
    printf("chip_pin_mode \n");
    uint8_t pin = arg1.uint32;
    uint8_t mode = arg0.uint32;
    pinMode(pin, mode);
    pop_args(2);
}

// warning: undefined symbol: chip_digital_write
def_prim(chip_digital_write, twoToNoneU32) {
    yield();
    uint8_t pin = arg1.uint32;
    uint8_t val = arg0.uint32;
    digitalWrite(pin, val);
    pop_args(2);
}

def_prim(chip_delay, oneToNoneU32) {
    printf("chip_delay \n");
    delay(arg0.uint32);
    pop_args(1);
}

//warning: undefined symbol: chip_delay_us
def_prim (chip_delay_us, oneToNoneU32) {
    yield();
    delay_us(arg0.uint32);
    pop_args(1);
}

def_prim(chip_digital_read, oneToOneU32) {
    uint8_t pin = arg0.uint32;
    uint8_t res = digitalRead(pin);
    pushInt32(res);
}

//warning: undefined symbol: write_spi_byte
def_prim (write_spi_byte, oneToNoneU32) {
    write_spi_byte(arg0.uint32);
    pop_args(1);
}

//warning: undefined symbol: spi_begin
def_prim (spi_begin, NoneToNoneU32) {
    yield();
    printf("spi_begin \n");
    spi->begin();
}

def_prim(write_spi_bytes_16,twoToNoneU32) {
        write_spi_bytes_16_prim(arg1.uint32,arg0.uint32);
    pop_args(2);
}

#else

def_prim(assert_int, oneToNoneU32) {
    uint8_t boolean = arg0.uint32;
    dbg_trace("EMU: assert(%u) \n", boolean);
    pop_args(1);
    return (bool) boolean;
}

def_prim(chip_pin_mode, twoToNoneU32) {
    dbg_trace("EMU: chip_pin_mode(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
}

def_prim(chip_digital_write, twoToNoneU32) {
    dbg_trace("EMU: chip_digital_write(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
}

def_prim(chip_delay, oneToNoneU32) {
    using namespace std::this_thread;  // sleep_for, sleep_until
    using namespace std::chrono;       // nanoseconds, system_clock, seconds
    dbg_trace("EMU: chip_delay(%u) \n", arg0.uint32);
    sleep_for(milliseconds(arg0.uint32));
    dbg_trace("EMU: .. done\n");
    pop_args(1);
}

def_prim(chip_delay_us, oneToNoneU32) {
    using namespace std::this_thread;  // sleep_for, sleep_until
    using namespace std::chrono;       // nanoseconds, system_clock, seconds
    dbg_trace("EMU: chip_delay(%u ms) \n", arg0.uint32);
    sleep_for(microseconds(arg0.uint32));
    dbg_trace("EMU: .. done\n");
    pop_args(1);
}

//warning: undefined symbol: write_spi_byte
def_prim (write_spi_byte, oneToNoneU32) {
    dbg_trace("EMU: write_spi_byte(%u) \n", arg0.uint32);
    pop_args(1);
}

//warning: undefined symbol: spi_begin
def_prim (spi_begin, NoneToNoneU32) {
    dbg_trace("EMU: spi_begin \n");
}

def_prim(write_spi_bytes_16, twoToNoneU32) {
    dbg_trace("EMU: write_spi_byte_16(%u, %u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
}


#endif

/*
int analogRead(uint8_t pin)
void analogReference(uint8_t mode)
void analogWrite(uint8_t pin, int val)
void analogWriteFreq(uint32_t freq)
void analogWriteRange(uint32_t range)
*/

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives() {
    dbg_info("INSTALLING PRIMITIVES\n");
    //install_primitive(rand);
#ifdef ARDUINO
    dbg_info("INSTALLING ARDUINO\n");
    install_primitive(assert_int);
    install_primitive(chip_pin_mode);
    install_primitive(chip_digital_write);
    install_primitive(chip_delay);
    install_primitive(chip_digital_read);
    install_primitive(chip_delay_us);
    install_primitive(spi_begin);
    install_primitive(write_spi_byte);
    install_primitive(write_spi_bytes_16);
#else
    dbg_info("INSTALLING FAKE ARDUINO\n");
    install_primitive(assert_int);
    install_primitive(chip_pin_mode);
    install_primitive(chip_digital_write);
    install_primitive(chip_delay);
    install_primitive(chip_delay_us);
    install_primitive(spi_begin);
    install_primitive(write_spi_byte);
    install_primitive(write_spi_bytes_16);
#endif
}

//------------------------------------------------------
// resolving the primitives
//------------------------------------------------------
bool resolve_primitive(char *symbol, Primitive *val) {
    debug("Resolve primitives (%d) for %s  \n", ALL_PRIMITIVES, symbol);

    for (auto &primitive : primitives) {
        //printf("Checking %s = %s  \n", symbol, primitives[i].name);
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
            external_mem.bytes = (uint8_t *) acalloc(
                    external_mem.pages * PAGE_SIZE, sizeof(uint32_t),
                    "Module->memory.bytes primitive");
        }
        *val = &external_mem;
        return true;
    }

    FATAL("Could not find memory %s \n", symbol);
    return false;
}
