#include "../Interpreter/instructions.h"
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

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <thread>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/CallbackHandler.h"
#include "primitives.h"

#define NUM_GLOBALS 0
#define ALL_GLOBALS NUM_GLOBALS

int global_index = 0;

double sensor_emu = 0;

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

def_prim(init_pixels, NoneToNoneU32) {
    printf("init_pixels \n");
    return true;
}

def_prim(set_pixel_color, fourToOneU32) {
    printf("set_pixel_color \n");
    pop_args(4);
    return true;
}

def_prim(show_pixels, NoneToNoneU32) {
    printf("show pixels \n");
    return true;
}

def_prim(clear_pixels, NoneToNoneU32) {
    printf("clear pixels \n");
    return true;
}

def_prim(abort, NoneToNoneU32) {
    debug("EMU: abort\n");
    return false;
}

def_prim(millis, NoneToOneU64) {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    const uint64_t millis = 1000 * tv.tv_sec + tv.tv_usec / 1000;
    pushUInt64(millis);
    return true;
}

def_prim(micros, NoneToOneU64) {
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    unsigned long micros = 1000000 * tv.tv_sec + tv.tv_usec;
    pushUInt64(micros);
    return true;
}

// call callback test function (temporary)
def_prim(test, oneToNoneU32) {
    uint32_t fidx = arg0.uint32;

    std::string topic = "interrupt";
    topic.append(std::to_string(fidx));

    Callback c = Callback(m, topic, fidx);
    CallbackHandler::add_callback(c);
    auto *payload = reinterpret_cast<const unsigned char *>("TestPayload");
    CallbackHandler::push_event(topic, reinterpret_cast<const char *>(payload),
                                11);
    pop_args(1);
    return true;
}

def_prim(print_int, oneToNoneU32) {
    debug("EMU: print ");
    printf("%u\n", arg0.uint32);
    pop_args(1);
    return true;
}

def_prim(print_string, twoToNoneU32) {
    uint32_t addr = arg1.uint32;
    uint32_t size = arg0.uint32;
    std::string text = parse_utf8_string(m->memory.bytes, size, addr);
    debug("EMU: print string at %i: ", addr);
    printf("%s", text.c_str());
    pop_args(2);
    return true;
}

def_prim(wifi_connect, fourToNoneU32) {
    uint32_t ssid = arg3.uint32;
    uint32_t len0 = arg2.uint32;
    uint32_t pass = arg1.uint32;
    uint32_t len1 = arg0.uint32;

    std::string ssid_str = parse_utf8_string(m->memory.bytes, len0, ssid);
    std::string pass_str = parse_utf8_string(m->memory.bytes, len1, pass);
    debug("EMU: connect to %s with password %s\n", ssid_str.c_str(),
          pass_str.c_str());
    pop_args(4);
    return true;
}

def_prim(wifi_status, NoneToOneU32) {
    pushInt32(3);  // return WL_CONNECTED
    return true;
}

def_prim(wifi_connected, NoneToOneU32) {
    pushInt32(1);  // return that we are connected
    return true;
}

def_prim(wifi_localip, twoToOneU32) {
    uint32_t buff = arg1.uint32;
    // uint32_t size = arg0.uint32; // never used in emulator
    std::string ip = "192.168.0.181";

    for (unsigned long i = 0; i < ip.length(); i++) {
        m->memory.bytes[buff + i] = *reinterpret_cast<uint8_t *>(&ip[i]);
    }
    pop_args(2);
    pushInt32(buff);
    return true;
}

def_prim(http_get, fourToOneU32) {
    // Get arguments
    uint32_t url = arg3.uint32;
    uint32_t length = arg2.uint32;
    int32_t response = arg1.uint32;
    uint32_t size = arg0.uint32;
    // Parse url
    std::string text = parse_utf8_string(m->memory.bytes, length, url);
    debug("EMU: http get request %s\n", text.c_str());
    // Construct response
    std::string answer = "Response code: 200.";
    if (answer.length() > size) {
        sprintf(exception, "GET: buffer size is too small for response.");
        return false;  // TRAP
    }
    for (unsigned long i = 0; i < answer.length(); i++) {
        m->memory.bytes[response + i] =
            *reinterpret_cast<unsigned char *>(&answer[i]);
    }

    // Pop args and return response address
    pop_args(4);
    pushInt32(response);
    return true;
}

def_prim(http_post, tenToOneU32) {
    // Get arguments
    uint32_t url = arg9.uint32;
    uint32_t url_len = arg8.uint32;
    uint32_t body = arg7.uint32;
    uint32_t body_len = arg6.uint32;
    uint32_t content_type = arg5.uint32;
    uint32_t content_type_len = arg4.uint32;
    uint32_t authorization = arg3.uint32;
    uint32_t authorization_len = arg2.uint32;
    int32_t response = arg1.uint32;
    // uint32_t size = arg0.uint32; // never used in emulator

    std::string url_parsed = parse_utf8_string(m->memory.bytes, url_len, url);
    std::string body_parsed =
        parse_utf8_string(m->memory.bytes, body_len, body);
    std::string content_type_parsed =
        parse_utf8_string(m->memory.bytes, content_type_len, content_type);
    std::string authorization_parsed =
        parse_utf8_string(m->memory.bytes, authorization_len, authorization);
    debug(
        "EMU: POST %s\n\t Content-type: '%s'\n\t Authorization: '%s'\n\t "
        "'%s'\n",
        url_parsed.c_str(), content_type_parsed.c_str(),
        authorization_parsed.c_str(), body_parsed.c_str());

    pop_args(10);
    pushInt32(response);
    return true;
}

#define NUM_DIGITAL_PINS 100
static uint32_t PINS[NUM_DIGITAL_PINS] = {};
static uint8_t MODES[NUM_DIGITAL_PINS] = {};

def_prim(chip_pin_mode, twoToNoneU32) {
    uint8_t pin = arg1.uint32;
    uint8_t val = arg0.uint32;
    if (pin < NUM_DIGITAL_PINS) {
        MODES[pin] = val;
    }
    pop_args(2);
    printf("EMU: chip_pin_mode(%u,%u) \n", pin, val);
    return pin < NUM_DIGITAL_PINS;
}

def_prim(chip_digital_write, twoToNoneU32) {
    uint8_t pin = arg1.uint32;
    uint8_t val = arg0.uint32;
    printf("EMU: chip_digital_write(%u,%u) \n", pin, val);
    bool writable = pin < NUM_DIGITAL_PINS && MODES[pin] == 0x02;
    if (writable) {
        PINS[pin] = val;
    }
    printf("EMU: ");
    for (int i = 0; i < NUM_DIGITAL_PINS / 2; i++) {
        printf("%d ", PINS[i]);
    }
    printf("\nEMU: ");
    for (int i = NUM_DIGITAL_PINS / 2; i < NUM_DIGITAL_PINS; i++) {
        printf("%d ", PINS[i]);
    }
    printf("\n");
    pop_args(2);
    return writable;
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
    for (int pin = 0; pin < NUM_DIGITAL_PINS; pin++) {
        auto *state = new IOStateElement();
        state->key = "p" + std::to_string(pin);
        state->output = MODES[pin] == 0x02;
        state->value = PINS[pin];
        external_state.push_back(state);
    }
}

def_prim(chip_digital_read, oneToOneU32) {
    uint8_t pin = arg0.uint32;
    pop_args(1);
    if (pin < NUM_DIGITAL_PINS) {
        pushUInt32(PINS[pin]);
        return true;
    }
    return false;
}

def_prim(chip_analog_read, oneToOneI32) {
    // uint8_t pin = arg0.uint32; // never used in emulator
    pop_args(1);
    pushInt32(sin(sensor_emu) * 100);
    sensor_emu += .25;
    return true;
}

def_prim(chip_analog_write, twoToNoneU32) {
    debug("EMU: chip_analog_write(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(chip_delay, oneToNoneU32) {
    using namespace std::this_thread;  // sleep_for, sleep_until
    using namespace std::chrono;       // nanoseconds, system_clock, seconds
    debug("EMU: chip_delay(%u) \n", arg0.uint32);
    sleep_for(milliseconds(arg0.uint32));
    debug("EMU: .. done\n");
    pop_args(1);
    return true;
}

def_prim(chip_delay_us, oneToNoneU32) {
    using namespace std::this_thread;  // sleep_for, sleep_until
    using namespace std::chrono;       // nanoseconds, system_clock, seconds
    debug("EMU: chip_delay(%u ms) \n", arg0.uint32);
    sleep_for(microseconds(arg0.uint32));
    debug("EMU: .. done\n");
    pop_args(1);
    return true;
}

// warning: undefined symbol: write_spi_byte
def_prim(write_spi_byte, oneToNoneU32) {
    debug("EMU: write_spi_byte(%u) \n", arg0.uint32);
    pop_args(1);
    return true;
}

// warning: undefined symbol: spi_begin
def_prim(spi_begin, NoneToNoneU32) {
    debug("EMU: spi_begin \n");
    return true;
}

def_prim(write_spi_bytes_16, twoToNoneU32) {
    debug("EMU: write_spi_byte_16(%u, %u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(drive_motor, twoToNoneU32) {
    const int32_t speed = arg0.int32;
    const uint32_t motor_index = arg1.uint32;
    printf("EMU: drive_motor(%d, %d)\n", motor_index, speed);
    pop_args(2);
    return true;
}

def_prim(stop_motor, oneToNoneU32) {
    uint32_t motor_index = arg0.uint32;
    printf("EMU: stop_motor(%d)\n", motor_index);
    pop_args(1);
    return true;
}

def_prim(drive_motor_ms, threeToNoneU32) {
    const int32_t time = arg0.uint32;
    const int32_t speed = arg1.int32;
    const int32_t motor_index = arg2.int32;
    printf("EMU: drive_motor_ms(%d, %d, %d)\n", motor_index, speed, time);
    pop_args(3);

    return true;
}

def_prim(drive_motor_degrees, threeToNoneU32) {
    int32_t degrees = arg0.int32;
    int32_t speed = arg1.int32;
    uint32_t motor_index = arg2.uint32;
    printf("EMU: drive_motor_degrees(%d, %d, %d)\n", motor_index, speed,
           degrees);
    pop_args(3);
    return true;
}

def_prim(setup_uart_sensor, twoToNoneU32) {
    printf("EMU: setup_uart_sensor(%d, %d)\n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(read_uart_sensor, oneToOneI32) {
    printf("EMU: read_uart_sensor(%d)\n", arg0.uint32);
    pop_args(1);
    pushInt32(0);
    return true;
}

std::random_device r;
std::default_random_engine e(r());
std::uniform_int_distribution<int16_t> adc_dist(0, 1 << 12);  // 12 bit adc

def_prim(nxt_touch_sensor, oneToOneU32) {
    const uint32_t port = arg0.uint32;
    const int16_t v = adc_dist(e);
    pop_args(1);
    printf("nxt_touch_sensor(%u) = %d\n", port, v < 2000);
    pushUInt32(v < 2000);
    return true;
}

def_prim(ev3_touch_sensor, oneToOneU32) {
    const uint32_t port = arg0.uint32;
    const int16_t v = adc_dist(e);
    pop_args(1);
    printf("ev3_touch_sensor(%u) = %d\n", port, v > 3000);
    pushUInt32(v > 3000);
    return true;
}

def_prim(subscribe_interrupt, threeToNoneU32) {
    uint8_t pin = arg2.uint32;   // GPIOPin
    uint8_t tidx = arg1.uint32;  // Table Idx pointing to Callback function
    [[maybe_unused]] uint8_t mode =
        arg0.uint32;  // Not used by emulator only printed

    debug("EMU: subscribe_interrupt(%u, %u, %u) \n", pin, tidx, mode);

    if (m->table.size < tidx) {
        debug("subscribe_interrupt: out of range table index %i\n", tidx);
        return false;
    }

    std::string topic = "interrupt_";
    topic.append(std::to_string(pin));

    Callback c = Callback(m, topic, tidx);
    CallbackHandler::add_callback(c);
    pop_args(3);
    return true;
}

// Temporary Primitives needed for analogWrite in ESP32
def_prim(chip_ledc_set_duty, threeToNoneU32) {
    uint8_t channel = arg2.uint32;
    uint32_t value = arg1.uint32;
    uint32_t maxValue = arg0.uint32;
    // calculate duty, 4095 from 2 ^ 12 - 1
    printf("chip_analog_write(%u, %u, %u)\n", channel, value, maxValue);
    pop_args(3);
    return true;
}

def_prim(chip_ledc_setup, threeToNoneU32) {
    uint32_t channel = arg2.uint32;
    uint32_t freq = arg1.uint32;
    uint32_t ledc_timer = arg0.uint32;
    printf("chip_ledc_setup(%u, %u, %u)\n", channel, freq, ledc_timer);
    pop_args(3);
    return true;
}

def_prim(chip_ledc_attach_pin, twoToNoneU32) {
    uint32_t pin = arg1.uint32;
    uint32_t channel = arg0.uint32;
    printf("chip_ledc_attach_pin(%u,%u)\n", pin, channel);
    pop_args(2);
    return true;
}

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives(Interpreter *interpreter) {
    dbg_info("INSTALLING PRIMITIVES\n");
    dbg_info("INSTALLING FAKE ARDUINO\n");
    install_primitive(abort);
    install_primitive(millis);
    install_primitive(micros);

    install_primitive(print_int);
    install_primitive(print_string);

    install_primitive(wifi_connect);
    install_primitive(wifi_status);
    install_primitive(wifi_connected);
    install_primitive(wifi_localip);

    install_primitive(http_get);
    install_primitive(http_post);

    install_primitive(chip_pin_mode);
    install_reversible_primitive(chip_digital_write);
    install_primitive(chip_delay);
    install_primitive(chip_digital_read);
    install_primitive(chip_analog_read);
    install_primitive(chip_delay_us);

    install_primitive(spi_begin);
    install_primitive(write_spi_byte);
    install_primitive(write_spi_bytes_16);

    install_primitive(subscribe_interrupt);

    install_primitive(init_pixels);
    install_primitive(set_pixel_color);
    install_primitive(clear_pixels);
    install_primitive(show_pixels);

    // temporary mock primitives needed for analogWrite in ESP32
    install_primitive(chip_analog_write);
    install_primitive(chip_ledc_setup);
    install_primitive(chip_ledc_attach_pin);
    install_primitive(chip_ledc_set_duty);

    // Open Bot Brain
    install_primitive(drive_motor);
    install_primitive(drive_motor_ms);
    install_primitive(drive_motor_degrees);
    install_primitive(stop_motor);
    install_primitive(setup_uart_sensor);
    install_primitive(read_uart_sensor);
    install_primitive(nxt_touch_sensor);
    install_primitive(ev3_touch_sensor);
}

Memory external_mem{};

// ReSharper disable once CppDFAConstantFunctionResult
bool resolve_external_memory(char *symbol, Memory **val) {
    if (!strcmp(symbol, "memory")) {
        if (external_mem.bytes == nullptr) {
            external_mem.initial = 256;
            external_mem.maximum = 256;
            external_mem.pages = 256;
            external_mem.bytes = static_cast<uint8_t *>(
                acalloc(external_mem.pages * PAGE_SIZE, sizeof(uint32_t),
                        "Module->memory.bytes primitive"));
        }
        *val = &external_mem;
        return true;
    }

    FATAL("Could not find memory %s \n", symbol);
    // return false; // unreachable
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
    // return false; // unreachable
}

#endif  // ARDUINO
