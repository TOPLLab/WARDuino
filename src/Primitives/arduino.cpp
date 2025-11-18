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
#include "Arduino.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <sys/time.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/CallbackHandler.h"
#include "primitives.h"

// M5stack
#include "m5stack/display.h"

#define delay_us(ms) delayMicroseconds(ms)
#include <SPI.h>
SPIClass *spi = new SPIClass();

// Hardware SPI
void write_spi_byte(unsigned char c) {
    spi->beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    spi->transfer(c);
    spi->endTransaction();
}

void write_spi_bytes_16_prim(int times, uint32_t color) {
    unsigned char colorB = color >> 8;
    spi->beginTransaction(SPISettings(200000000, MSBFIRST, SPI_MODE0));
    for (int x = 0; x < times; x++) {
        spi->transfer(colorB);
        spi->transfer(color);
    }
    spi->endTransaction();
}

// Hardware Interrupts

#define ALL_ISRS 26  // number of installed ISRs

typedef struct ISREntry {
    int pin;
    void (*ISR_callback)();
} ISREntry;

ISREntry ISRs[ALL_ISRS];
int isr_index = 0;

/* Private macro to install an ISR */
#define install_isr(number)                                         \
    {                                                               \
        dbg_info("installing isr number: %d of %d with name: %s\n", \
                 isr_index + 1, ALL_ISRS, isr_##number);            \
        if (isr_index < ALL_ISRS) {                                 \
            ISREntry *p = &ISRs[isr_index++];                       \
            p->pin = number;                                        \
            p->ISR_callback = &(isr_##number);                      \
        } else {                                                    \
            FATAL("isr_index out of bounds");                       \
        }                                                           \
    }

#define INTERRUPT_TOPIC_PREFIX "interrupt_"

/* Private macro to create an ISR for a specific pin*/
#define def_isr(pin)                                                     \
    void isr_##pin() {                                                   \
        CallbackHandler::push_event(INTERRUPT_TOPIC_PREFIX #pin, "", 0); \
    }

/* Common GPIO pins on ESP32 devices:*/
def_isr(1);
def_isr(2);
def_isr(3);
def_isr(4);
def_isr(5);
def_isr(12);
def_isr(13);
def_isr(14);
def_isr(15);
def_isr(16);
def_isr(17);
def_isr(18);
def_isr(19);
def_isr(21);
def_isr(22);
def_isr(23);
def_isr(25);
def_isr(26);
def_isr(27);
def_isr(32);
def_isr(33);
def_isr(34);
def_isr(35);
def_isr(36);
def_isr(37);
def_isr(39);

int resolve_isr(int pin) {
    debug("Resolve ISR (%d) for %i \n", ALL_ISRS, pin);

    for (int i = 0; i < ALL_ISRS; i++) {
        auto &isr = ISRs[i];
        debug("Checking entry %i of %i: pin = %i \n", i, ALL_ISRS, isr.pin);
        if (pin == isr.pin) {
            debug("FOUND ISR\n");
            return i;
        }
    }
    return -1;
}

// Primitives

#define NUM_PRIMITIVES 0
#define NUM_PRIMITIVES_ARDUINO 44

#define ALL_PRIMITIVES (NUM_PRIMITIVES + NUM_PRIMITIVES_ARDUINO)

// Global index for installing primitives
int prim_index = 0;

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
            p->t = &(prim_name##_type);                                    \
            p->f = &(prim_name);                                           \
            p->f_reverse = nullptr;                                        \
            p->f_serialize_state = nullptr;                                \
        } else {                                                           \
            FATAL("prim_index out of bounds");                             \
        }                                                                  \
    }

#define install_primitive_reverse(prim_name)             \
    {                                                    \
        PrimitiveEntry *p = &primitives[prim_index - 1]; \
        p->f_reverse = &(prim_name##_reverse);           \
        p->f_serialize_state = &(prim_name##_serialize); \
    }

#define def_prim(function_name, type) \
    Type function_name##_type = type; \
    bool function_name(Module *m)

#define def_prim_reverse(function_name)     \
    void function_name##_reverse(Module *m, \
                                 std::vector<IOStateElement> external_state)

#define def_prim_serialize(function_name) \
    void function_name##_serialize(       \
        std::vector<IOStateElement *> &external_state)

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
uint32_t param_I32_arr_len5[5] = {I32, I32, I32, I32, I32};
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

Type fiveToNoneI32 = {
    .form = FUNC,
    .param_count = 5,
    .params = param_I32_arr_len5,
    .result_count = 0,
    .results = nullptr,
    .mask = 0x80011111
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

// Util function declarations

void connect(const String ssid, const String password);

int32_t http_get_request(Module *m, const String url, uint32_t response,
                         uint32_t size);

int32_t http_post_request(Module *m, const String url, const String body,
                          const String content_type,
                          const String authorization_parsed, uint32_t response,
                          uint32_t size);

//------------------------------------------------------
// Arduino Specific Functions
//------------------------------------------------------

def_prim(abort, NoneToNoneU32) {
    sprintf(exception, "Trap: assertion failed.");
    return false;
}

def_prim(millis, NoneToOneU32) {
    pushInt32(millis());
    return true;
}

def_prim(micros, NoneToOneU64) {
    pushUInt64(micros());
    return true;
}

def_prim(int_random, NoneToOneU32) {
    pushInt32(random(6000));
    return true;
}

def_prim(print_int, oneToNoneU32) {
    uint32_t integer = arg0.uint32;
    Serial.print(integer);
    Serial.flush();
    pop_args(1);
    return true;
}

def_prim(print_string, twoToNoneU32) {
    uint32_t addr = arg1.uint32;
    uint32_t size = arg0.uint32;

    String str = parse_utf8_string(m->memory.bytes, size, addr).c_str();

    Serial.print(str);
    Serial.flush();
    pop_args(2);
    return true;
}

// warning: undefined symbol: chip_pin_mode
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

def_prim_reverse(chip_digital_write) {
    for (IOStateElement state : external_state) {
        if (!state.output) {
            continue;
        }

        if (state.key[0] == 'p') {
            invoke_primitive(m, "chip_digital_write", stoi(state.key.substr(1)),
                             (uint32_t)state.value);
        }
    }
}

def_prim_serialize(chip_digital_write) {
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
        uint32_t bit_mask = digitalPinToBitMask(i);
        auto *state = new IOStateElement();
        state->key = "p" + std::to_string(i);
        uint8_t port = digitalPinToPort(i);
        if (*portModeRegister(port) & bit_mask) {  // DDR
            state->output = true;
            state->value = (*portOutputRegister(port) & bit_mask) > 0;
        } else {
            state->output = false;
            state->value = (*portInputRegister(port) & bit_mask) > 0;
        }
        external_state.push_back(state);
    }
}

def_prim(chip_delay, oneToNoneU32) {
    delay(arg0.uint32);
    pop_args(1);
    return true;
}

// warning: undefined symbol: chip_delay_us
def_prim(chip_delay_us, oneToNoneU32) {
    yield();
    delay_us(arg0.uint32);
    pop_args(1);
    return true;
}

def_prim(chip_digital_read, oneToOneU32) {
    uint8_t pin = arg0.uint32;
    uint8_t res = digitalRead(pin);
    pop_args(1);
    pushUInt32(res);
    return true;
}

def_prim(chip_analog_read, oneToOneI32) {
    uint8_t pin = arg0.uint32;
    pop_args(1);
    pushInt32(analogRead(pin));
    return true;
}

def_prim(chip_analog_write, twoToNoneU32) {
    uint8_t pin = arg1.uint32;
    uint8_t brightness = arg0.uint32;
    pop_args(2);
    analogWrite(pin, brightness);
    return true;
}

// warning: undefined symbol: write_spi_byte
def_prim(write_spi_byte, oneToNoneU32) {
    write_spi_byte(arg0.uint32);
    pop_args(1);
    return true;
}

// warning: undefined symbol: spi_begin
def_prim(spi_begin, NoneToNoneU32) {
    yield();
    printf("spi_begin \n");
    spi->begin();
    return true;
}

def_prim(write_spi_bytes_16, twoToNoneU32) {
    write_spi_bytes_16_prim(arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

// LED Control primitives

def_prim(chip_ledc_set_duty, threeToNoneU32) {
    uint8_t channel = arg2.uint32;
    uint32_t value = arg1.uint32;
    uint32_t maxValue = arg0.uint32;

    // printf("chip_ledc_analog_write(%u, %u, %u)\n", channel, value, maxValue);
    // calculate duty, 4095 from 2 ^ 12 - 1
    uint32_t duty = (4095 / maxValue) * min(value, maxValue);

    ledcWrite(channel, duty);
    pop_args(3);
    return true;
}

def_prim(chip_ledc_attach, threeToNoneU32) {
    uint32_t channel = arg2.uint32;
    uint32_t freq = arg1.uint32;
    uint32_t resolution = arg0.uint32;
    ledcAttach(channel, freq, resolution);
    pop_args(3);
    return true;
}

// INTERRUPTS

def_prim(subscribe_interrupt, threeToNoneU32) {
    uint8_t pin = arg2.uint32;   // GPIOPin
    uint8_t tidx = arg1.uint32;  // Table Idx pointing to Callback function
    uint8_t mode = arg0.uint32;

    dbg_info("subscribe_interrupt(%i, %i, %i)\n", pin, tidx, mode);

    int index = resolve_isr(pin);
    if (index < 0) {
        dbg_info("subscribe_interrupt: no ISR found for pin %i\n", pin);
        return false;
    }

    if (tidx < 0 || m->table.size < tidx) {
        dbg_info("subscribe_interrupt: out of range table index %i\n", tidx);
        return false;
    }

    if (tidx < 0 || m->table.size < tidx) {
        dbg_info("subscribe_interrupt: out of range table index %i\n", tidx);
        return false;
    }

    attachInterrupt(digitalPinToInterrupt(pin), ISRs[index].ISR_callback, mode);

    String callback_id = INTERRUPT_TOPIC_PREFIX;
    callback_id += String(pin);
    Callback c = Callback(m, callback_id.c_str(), tidx);
    CallbackHandler::add_callback(c);

    pop_args(3);
    return true;
}

def_prim(unsubscribe_interrupt, oneToNoneU32) {
    uint8_t pin = arg0.uint32;

    detachInterrupt(digitalPinToInterrupt(pin));

    pop_args(1);
    return true;
}

//------------------------------------------------------
// Util functions
//------------------------------------------------------
void connect(const String ssid, const String password) {
    char *ssid_buf =
        (char *)acalloc(ssid.length() + 1, sizeof(char), "ssid_buf");
    ssid.toCharArray(ssid_buf, ssid.length() + 1);
    char *pass_buf =
        (char *)acalloc(password.length() + 1, sizeof(char), "pass_buf");
    password.toCharArray(pass_buf, password.length() + 1);

    WiFi.begin(ssid_buf, pass_buf);

    Serial.println("Connecting to wifi network.");
    Serial.flush();

    free(ssid_buf);
    free(pass_buf);
}

int32_t http_get_request(Module *m, const String url, const uint32_t response,
                         const uint32_t size) {
    HTTPClient http;
    int32_t httpResponseCode = 0;

    http.begin(url.c_str());
    httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
        printf("HTTP Response code: %i\n", httpResponseCode);
        String payload = http.getString();
        if (payload.length() > size) {
            sprintf(exception,
                    "GET: buffer size is too small for response of %i bytes.",
                    payload.length());
            return false;  // TRAP
        }
        for (unsigned long i = 0; i < payload.length(); i++) {
            m->memory.bytes[response + i] = (uint32_t)payload[i];
        }
    } else {
        printf("Error code: %i\n", httpResponseCode);
    }
    // Free resources
    http.end();

    return httpResponseCode;
}

int32_t http_post_request(Module *m, const String url, const String body,
                          const String contentType,
                          const String authorizationToken,
                          const uint32_t response, const uint32_t size) {
    HTTPClient http;
    int32_t httpResponseCode = 0;

    http.begin(url.c_str());
    http.addHeader("Authorization", authorizationToken.c_str());
    httpResponseCode = http.POST(body);

    if (httpResponseCode > 0) {
        Serial.print("Status code: ");
        Serial.print(httpResponseCode);

        if (httpResponseCode == 204) {
            Serial.println("");
            http.end();
            return httpResponseCode;
        }

        String responseBody = http.getString();

        Serial.println(" Response: ");
        Serial.println(responseBody);

        for (unsigned long i = 0; i < responseBody.length();
             i++) {  // TODO check size
            m->memory.bytes[response + i] = (uint32_t)responseBody[i];
        }
    } else {
        printf("Error code: %i\n", httpResponseCode);
    }

    // Free resources
    http.end();

    return httpResponseCode;
}

// Cardputer display
def_prim(display_setup, NoneToNoneU32) {
    displaySetup();
    return true;
}

def_prim(display_width, NoneToOneU32) {
    pushInt32(width());
    return true;
}

def_prim(display_height, NoneToOneU32) {
    pushInt32(height());
    return true;
}


def_prim(display_fillrect, fiveToNoneI32) {
    uint32_t color = arg4.uint32;
    int32_t height = arg3.int32;
    int32_t width = arg2.int32;
    int32_t y = arg1.int32;
    int32_t x = arg0.int32;
    fillRect(x, y, width, height, color);
    pop_args(5);
    return true;
}

def_prim(display_fillcircle, fourToNoneU32) {
    uint32_t color = arg3.uint32;
    int32_t radius = arg2.int32;
    int32_t y = arg1.int32;
    int32_t x = arg0.int32;
    fillCircle(x, y, radius, color);
    pop_args(5);
    return true;
}

//------------------------------------------------------
// Installing all the primitives & ISRs
//------------------------------------------------------
void install_isrs() {
    install_isr(1);
    install_isr(2);
    install_isr(3);
    install_isr(4);
    install_isr(5);
    install_isr(12);
    install_isr(13);
    install_isr(14);
    install_isr(15);
    install_isr(16);
    install_isr(17);
    install_isr(18);
    install_isr(19);
    install_isr(21);
    install_isr(22);
    install_isr(23);
    install_isr(25);
    install_isr(26);
    install_isr(27);
    install_isr(32);
    install_isr(33);
    install_isr(34);
    install_isr(35);
    install_isr(36);
    install_isr(37);
    install_isr(39);
}

void install_primitives() {
    dbg_info("INSTALLING PRIMITIVES\n");
    install_primitive(abort);
    install_primitive(millis);
    install_primitive(micros);
    install_primitive(int_random);

    install_primitive(print_int);
    install_primitive(print_string);

    install_primitive(chip_pin_mode);
    install_primitive(chip_digital_write);
    install_primitive_reverse(chip_digital_write);
    install_primitive(chip_delay);
    install_primitive(chip_digital_read);
    install_primitive(chip_analog_read);
    install_primitive(chip_delay_us);

    install_primitive(spi_begin);
    install_primitive(write_spi_byte);
    install_primitive(write_spi_bytes_16);

    install_primitive(subscribe_interrupt);
    install_primitive(unsubscribe_interrupt);

    // temporary primitives needed for analogWrite in ESP32
    install_primitive(chip_analog_write);
    install_primitive(chip_ledc_attach);
    install_primitive(chip_ledc_set_duty);

    install_primitive(display_setup);
    install_primitive(display_width);
    install_primitive(display_height);
    install_primitive(display_fillrect);
    install_primitive(display_fillcircle);

    dbg_info("INSTALLING ISRs\n");
    install_isrs();
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

Memory external_mem{};

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

//------------------------------------------------------
// Restore external state when restoring a snapshot
//------------------------------------------------------
void restore_external_state(Module *m,
                            const std::vector<IOStateElement> &external_state) {
    uint8_t opcode = *m->pc_ptr;
    // TODO: Maybe primitives can also be called using the other call
    // instructions such as call_indirect
    //  maybe there should just be a function that checks if a certain function
    //  is being called that handles all these cases?
    if (opcode == 0x10) {  // call opcode
        uint8_t *pc_copy = m->pc_ptr + 1;
        uint32_t fidx = read_LEB_32(&pc_copy);
        if (fidx < m->import_count) {
            for (auto &primitive : primitives) {
                if (!strcmp(primitive.name, m->functions[fidx].import_field)) {
                    if (primitive.f_reverse) {
                        debug("Reversing action for primitive %s\n",
                              primitive.name);
                        primitive.f_reverse(m, external_state);
                    }
                    return;
                }
            }
        }
    }
}

//------------------------------------------------------
// Serialize external state into a snapshot
//------------------------------------------------------
std::vector<IOStateElement *> get_io_state(Module *m) {
    std::vector<IOStateElement *> ioState;
    for (auto &primitive : primitives) {
        if (primitive.f_serialize_state) {
            primitive.f_serialize_state(ioState);
        }
    }
    return ioState;
}
