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
#include "glue.h"

#ifdef ARDUINO
#include "Arduino.h"
#include <WiFi.h>
#include <HTTPClient.h>

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

#include <cstdio>
#include <cstring>

#define NUM_PRIMITIVES 0
#ifdef ARDUINO
#define NUM_PRIMITIVES_ARDUINO 14
#else
#define NUM_PRIMITIVES_ARDUINO 13
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
#define arg3 get_arg(m, 3)

// The primitive table
PrimitiveEntry primitives[ALL_PRIMITIVES];

//
uint32_t param_arr_len0[0] = {};
uint32_t param_I32_arr_len1[1] = {I32};
uint32_t param_I32_arr_len2[2] = {I32, I32};
uint32_t param_I32_arr_len3[3] = {I32, I32, I32};
uint32_t param_I32_arr_len4[4] = {I32, I32, I32, I32};

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

Type fourToNoneU32 = {
        .form =  FUNC,
        .param_count =  4,
        .params =  param_I32_arr_len4,
        .result_count =  0,
        .results =  nullptr,
        .mask =  0x8001111 /* 0x800 = no return ; 1 = I32; 1 = I32; 1 = I32; 1 = I32*/
};

Type oneToOneU32 = {
        .form =  FUNC,
        .param_count =  1,
        .params =  param_I32_arr_len1,
        .result_count =  1,
        .results =  param_I32_arr_len1,
        .mask =  0x80011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

Type twoToOneU32 = {
        .form =  FUNC,
        .param_count =  2,
        .params =  param_I32_arr_len2,
        .result_count =  1,
        .results =  param_I32_arr_len1,
        .mask =  0x81011 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32*/
};

Type threeToOneU32 = {
        .form =  FUNC,
        .param_count =  3,
        .params =  param_I32_arr_len3,
        .result_count =  1,
        .results =  param_I32_arr_len1,
        .mask =  0x810111 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32; 1=I32*/
};

Type fourToOneU32 = {
        .form =  FUNC,
        .param_count =  4,
        .params =  param_I32_arr_len4,
        .result_count =  0,
        .results =  param_I32_arr_len1,
        .mask =  0x8101111 /* 0x8 1=I32 0=endRet ; 1=I32; 1=I32; 1=I32; 1=I32*/
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

def_prim(abort, NoneToNoneU32) {
    sprintf(exception, "Trap: assertion failed.");
    return false;
}

def_prim(print_int, oneToNoneU32) {
    uint32_t integer = arg0.uint32;
    Serial.print("Printing integer: ");
    Serial.println(integer);
    Serial.flush();
    pop_args(1);
    return true;
}

def_prim(print_string, oneToNoneU32) {
    Serial.println("print_string: ");

    uint32_t addr = arg0.uint32;
//    if (m->memory.bytes[addr + length - 1] != 0) {
//        URL isn't null-terminated
//        sprintf(exception, "Print-string: string isn't null-terminated.");
//        return false;
//    }

    String str = parse_ts_string(m->memory.bytes, m->memory.pages * PAGE_SIZE, addr).c_str();
    Serial.println(str);
    Serial.flush();
    pop_args(1);
    return true;
}

def_prim(connect, fourToNoneU32) {
    uint32_t ssid = arg3.uint32;
    uint32_t len0 = arg2.uint32;
    uint32_t pass = arg1.uint32;
    uint32_t len1 = arg0.uint32;

    if (m->memory.bytes[ssid + len0 - 1] != 0 && m->memory.bytes[pass + len1 - 1] != 0) {
        // One of the strings isn't null-terminated
        sprintf(exception, "Connect: string isn't null-terminated.");
        return false;
    }

    String ssid_str = parse_ts_string(m->memory.bytes, len0, ssid).c_str();
    String pass_str = parse_ts_string(m->memory.bytes, len1, pass).c_str();
    Serial.print("SSID: ");
    Serial.println(ssid_str);
//    Serial.print("PASS: ");
//    Serial.println(pass_str);

    char *ssid_buf = (char *) acalloc(ssid_str.length() + 1, sizeof(char), "ssid_buf");
    ssid_str.toCharArray(ssid_buf, ssid_str.length() + 1);
    char *pass_buf = (char *) acalloc(pass_str.length() + 1, sizeof(char), "pass_buf");
    pass_str.toCharArray(pass_buf, pass_str.length() + 1);
    WiFi.begin(ssid_buf, pass_buf);
    printf("Connecting to wifi\n");
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to WiFi network with IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.flush();

    free(ssid_buf);
    free(pass_buf);
    pop_args(4);
    return true;
}

def_prim(get, fourToOneU32) {
    uint32_t return_value = 0;
    //Check WiFi connection status
    if(WiFi.status()== WL_CONNECTED) {
        HTTPClient http;

        uint32_t addr = arg3.uint32;
        uint32_t length = arg2.uint32;
        uint32_t response = arg1.uint32;
        uint32_t size = arg0.uint32;
        // url string
        if (m->memory.bytes[addr + length - 1] != 0) {
            // URL isn't null-terminated
            sprintf(exception, "Get: string isn't null-terminated.");
            return false;
        }

        String url = parse_ts_string(m->memory.bytes, length, addr).c_str();
        Serial.print("GET ");
        Serial.println(url);

        // Send HTTP GET request
        http.begin(url.c_str());
        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            printf("HTTP Response code: %i\n", httpResponseCode);
            String payload = http.getString();
            if (payload.length() > size)    {
                sprintf(exception, "GET: buffer size is too small for response of %i bytes.", payload.length());
                return false;  // TRAP
            }
            for (unsigned long i = 0; i < payload.length(); i++) {
                m->memory.bytes[response + (i * 2)] = (uint32_t) payload[i];
            }
            return_value = response;
        } else {
            printf("Error code: %i\n", httpResponseCode);
        }
        // Free resources
        http.end();
    }
    pop_args(3);
    pushInt32(return_value);
    Serial.flush();
    return true;
}

def_prim(post, NoneToNoneU32) {
    // TODO
    pop_args(0);
    return true;
}

//warning: undefined symbol: chip_pin_mode
def_prim(chip_pin_mode, twoToNoneU32) {
    printf("chip_pin_mode \n");
    uint8_t pin = arg1.uint32;
    uint8_t mode = arg0.uint32;
    pinMode(pin, mode);
    pop_args(2);
    return true;
}

// warning: undefined symbol: chip_digital_write
def_prim(chip_digital_write, twoToNoneU32) {
    yield();
    uint8_t pin = arg1.uint32;
    uint8_t val = arg0.uint32;
    digitalWrite(pin, val);
    pop_args(2);
    return true;
}

def_prim(chip_delay, oneToNoneU32) {
    printf("chip_delay \n");
    delay(arg0.uint32);
    pop_args(1);
    return true;
}

//warning: undefined symbol: chip_delay_us
def_prim (chip_delay_us, oneToNoneU32) {
    yield();
    delay_us(arg0.uint32);
    pop_args(1);
    return true;
}

def_prim(chip_digital_read, oneToOneU32) {
    uint8_t pin = arg0.uint32;
    uint8_t res = digitalRead(pin);
    pushInt32(res);
    return true;
}

//warning: undefined symbol: write_spi_byte
def_prim (write_spi_byte, oneToNoneU32) {
    write_spi_byte(arg0.uint32);
    pop_args(1);
    return true;
}

//warning: undefined symbol: spi_begin
def_prim (spi_begin, NoneToNoneU32) {
    yield();
    printf("spi_begin \n");
    spi->begin();
    return true;
}

def_prim(write_spi_bytes_16,twoToNoneU32) {
        write_spi_bytes_16_prim(arg1.uint32,arg0.uint32);
    pop_args(2);
    return true;
}

#else

def_prim(abort, NoneToNoneU32) {
    dbg_trace("EMU: abort\n");
    return false;
}

def_prim(print_int, oneToNoneU32) {
    dbg_trace("EMU: print %i\n", arg0.uint32);
    pop_args(1);
    return true;
}

def_prim(print_string, oneToNoneU32) {
    uint32_t addr = arg0.uint32;
    std::string text = parse_ts_string(m->memory.bytes, m->memory.pages * PAGE_SIZE, addr);
    dbg_trace("EMU: print string at %i: %s\n", addr, text.c_str());
    pop_args(1);
    return true;
}

def_prim(connect, fourToNoneU32) {
    uint32_t ssid = arg3.uint32;
    uint32_t len0 = arg2.uint32;
    uint32_t pass = arg1.uint32;
    uint32_t len1 = arg0.uint32;

    std::string ssid_str = parse_ts_string(m->memory.bytes, len0, ssid);
    std::string pass_str = parse_ts_string(m->memory.bytes, len1, pass);
    dbg_trace("EMU: connect to %s with password %s\n", ssid_str.c_str(), pass_str.c_str());
    pop_args(4);
    return true;
}

def_prim(get, fourToOneU32) {
    // Get arguments
    uint32_t url = arg3.uint32;
    uint32_t length = arg2.uint32;
    uint32_t response = arg1.uint32;
    uint32_t size = arg0.uint32;
    // Parse url
    std::string text = parse_ts_string(m->memory.bytes, length, url);
    dbg_trace("EMU: http get request %s\n", text.c_str());
    // Construct response
    std::string answer = "Response code: 200.";
    if (answer.length() > size)    {
        sprintf(exception, "GET: buffer size is too small for response.");
        return false;  // TRAP
    }
    for (unsigned long i = 0; i < answer.length(); i++) {
        m->memory.bytes[response + (i * 2)] = (uint32_t) answer[i];
    }
    // Pop args and return response address
    pop_args(3);
    pushInt32(response);
    return true;
}

def_prim(post, NoneToNoneU32) {
    // TODO
    pop_args(0);
    return true;
}

def_prim(chip_pin_mode, twoToNoneU32) {
    dbg_trace("EMU: chip_pin_mode(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(chip_digital_write, twoToNoneU32) {
    dbg_trace("EMU: chip_digital_write(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(chip_delay, oneToNoneU32) {
    using namespace std::this_thread;  // sleep_for, sleep_until
    using namespace std::chrono;       // nanoseconds, system_clock, seconds
    dbg_trace("EMU: chip_delay(%u) \n", arg0.uint32);
    sleep_for(milliseconds(arg0.uint32));
    dbg_trace("EMU: .. done\n");
    pop_args(1);
    return true;
}

def_prim(chip_delay_us, oneToNoneU32) {
    using namespace std::this_thread;  // sleep_for, sleep_until
    using namespace std::chrono;       // nanoseconds, system_clock, seconds
    dbg_trace("EMU: chip_delay(%u ms) \n", arg0.uint32);
    sleep_for(microseconds(arg0.uint32));
    dbg_trace("EMU: .. done\n");
    pop_args(1);
    return true;
}

//warning: undefined symbol: write_spi_byte
def_prim (write_spi_byte, oneToNoneU32) {
    dbg_trace("EMU: write_spi_byte(%u) \n", arg0.uint32);
    pop_args(1);
    return true;
}

//warning: undefined symbol: spi_begin
def_prim (spi_begin, NoneToNoneU32) {
    dbg_trace("EMU: spi_begin \n");
    return true;
}

def_prim(write_spi_bytes_16, twoToNoneU32) {
    dbg_trace("EMU: write_spi_byte_16(%u, %u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
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
    install_primitive(abort);
    install_primitive(print_int);
    install_primitive(print_string);
    install_primitive(connect);
    install_primitive(get);
    install_primitive(post);
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
    install_primitive(abort);
    install_primitive(print_int);
    install_primitive(print_string);
    install_primitive(connect);
    install_primitive(get);
    install_primitive(post);
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
