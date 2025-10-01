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
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/devicetree/gpio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util_macro.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "Mindstorms/Motor.h"
#include "Mindstorms/uart_sensor.h"
#include "primitives.h"

#define OBB_PRIMITIVES 0
#ifdef CONFIG_BOARD_STM32L496G_DISCO
#define OBB_PRIMITIVES 7
#endif
#define ALL_PRIMITIVES OBB_PRIMITIVES + 7

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

Type oneToNoneI32 = {
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
    k_yield();
    k_msleep(arg0.uint32);
    pop_args(1);
    return true;
}

struct gpio_dt_spec specs[] = {DT_FOREACH_PROP_ELEM_SEP(
    DT_PATH(zephyr_user), warduino_gpios, GPIO_DT_SPEC_GET_BY_IDX, (, ))};

def_prim(chip_pin_mode, twoToNoneU32) {
    printf("chip_pin_mode(%u,%u)\n", arg1.uint32, arg0.uint32);
    gpio_dt_spec pin_spec = specs[arg1.uint32];
    int err = gpio_pin_configure(pin_spec.port, pin_spec.pin,
                                 arg0.uint32 == 0 ? GPIO_INPUT : GPIO_OUTPUT);
    if (err < 0) {
        printf("Error configuring pin %d\n", err);
        return false;
    }
    pop_args(2);
    return true;
}

std::unordered_map<uint32_t, uint32_t> io_map;

def_prim(chip_digital_write, twoToNoneU32) {
    printf("chip_digital_write(%u,%u)\n", arg1.uint32, arg0.uint32);
    gpio_dt_spec pin_spec = specs[arg1.uint32];
    gpio_pin_set_raw(pin_spec.port, pin_spec.pin, arg0.uint32);
    io_map[arg1.uint32] = arg0.uint32;
    pop_args(2);
    return true;
}

def_prim_serialize(chip_digital_write) {
    for (auto pair : io_map) {
        IOStateElement *state = new IOStateElement();
        state->output = true;
        state->key = "p" + std::to_string(pair.first);
        state->value = pair.second;
        external_state.push_back(state);
    }
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

def_prim(chip_digital_read, oneToOneU32) {
    printf("chip_digital_read(%u)\n", arg0.uint32);
    gpio_dt_spec pin_spec = specs[arg0.uint32];
    uint8_t res = gpio_pin_get_raw(pin_spec.port, pin_spec.pin);
    pop_args(1);
    pushUInt32(res);
    return true;
}

int16_t read_value(int c, int *channels) {
    //printf("read_value(%u)\n", c);

    int16_t buf = -1;
    //struct adc_sequence seq = { .buffer = &buf, .buffer_size = sizeof(buf) };
    //const int channel = 6; // Port 2
    //const int channel = 7; // Port 3
    //const int channel = 8; // Port 4
    /*int *channels = new int[4]{
        12, // Port 1
        6, // Port 2
        7, // Port 3
        8, // Port 4
    };*/
    /*int *channels = new int[4]{
        13, // Port 1
        5, // Port 2
        9, // Port 3
        10, // Port 4
    };*/
    int channel = channels[c];
    if (channel < 0) {
        printf("Invalid channel for chip_analog_read(%d)\n", channel);
        return -1;
    }
    //printf("Configuring channel %d\n", channel);
    struct adc_sequence seq = {
        .channels = BIT(channel), // Assuming channel 0 is the one we want to read
        .buffer = &buf,
        .buffer_size = sizeof(buf),
        .resolution = 12, // 12-bit resolution
        //.oversampling = 0,
    };

    // Modify the adc on both lines here!
    const device *adc_dev;
    adc_channel_cfg cfg;

    if (c == 0) {
        adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc3));
        struct adc_channel_cfg channel_cfgs[] = {
            DT_FOREACH_CHILD_SEP(DT_NODELABEL(adc3), ADC_CHANNEL_CFG_DT, (,))};
        cfg = channel_cfgs[channel];
    }
    else {
        adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc1));
        struct adc_channel_cfg channel_cfgs[] = {
            DT_FOREACH_CHILD_SEP(DT_NODELABEL(adc1), ADC_CHANNEL_CFG_DT, (,))};
        cfg = channel_cfgs[channel];
    }

    int err = adc_channel_setup(adc_dev, &cfg);
    if (err < 0) {
        printf("Failed to setup ADC channel: %d\n", err);
        perror("failed to setup ADC channel");
        return -1;
    }

    err = adc_read(adc_dev, &seq);
    if (err < 0) {
        printf("Failed to read ADC channel: %d\n", err);
        perror("failed to read ADC channel");
        return -1;
    }

    printf("ADC read value(channel = %d): %d\n", channel, buf);
    return buf;
}

def_prim(chip_analog_read, oneToOneU32) {
    int channels[4] = {
        12, // Port 1
        6,  // Port 2
        7,  // Port 3
        8,  // Port 4
    };
    int16_t v = read_value(arg0.uint32, channels);
    pop_args(1);
    pushUInt32(v);
    return true;
}

def_prim(ev3_touch_sensor, oneToOneU32) {
    printf("ev3_touch_sensor(%u)\n", arg0.uint32);

    int channels[4] = {
        13, // Port 1
        5, // Port 2
        9, // Port 3
        10, // Port 4
    };
    int16_t v = read_value(arg0.uint32, channels);
    pop_args(1);
    pushUInt32(v > 3000);
    return true;
}

def_prim(print_int, oneToNoneI32) {
    printf("%d\n", arg0.int32);
    pop_args(1);
    return true;
}

def_prim(abort, NoneToNoneU32) {
    printf("abort\n");
    return false;
}

#ifdef CONFIG_BOARD_STM32L496G_DISCO

MotorEncoder *encoders[] = {new MotorEncoder(specs[51], specs[50], "Port A"),
                            new MotorEncoder(specs[57], specs[58], "Port B"),
                            new MotorEncoder(specs[17], specs[13], "Port C"),
                            new MotorEncoder(specs[27], specs[26], "Port D")};

#define PWM_SPEC_GETTER(node_id, prop, idx) \
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), idx)
struct pwm_dt_spec pwm_specs[] = {DT_FOREACH_PROP_ELEM_SEP(
    DT_PATH(zephyr_user), pwms, PWM_SPEC_GETTER, (, ))};

std::optional<Motor> get_motor(uint32_t motor_index) {
    if (motor_index > sizeof(encoders) / sizeof(*encoders)) {
        return {};
    }

    return std::make_optional<Motor>(pwm_specs[motor_index * 2 + 1],
                                     pwm_specs[motor_index * 2],
                                     encoders[motor_index]);
}

def_prim(drive_motor, twoToNoneU32) {
    int32_t speed = arg0.int32;
    uint32_t motor_index = arg1.uint32;
    printf("drive_motor(%d, %d)\n", motor_index, speed);
    pop_args(2);

    if (auto motor = get_motor(motor_index)) {
        motor.value().set_speed(speed / 10000.0f);
        return true;
    }

    printf("Invalid motor index %d\n", motor_index);
    return false;
}

def_prim(stop_motor, oneToNoneU32) {
    uint32_t motor_index = arg0.uint32;
    pop_args(1);

    if (auto motor = get_motor(motor_index)) {
        motor.value().halt();
        return true;
    }

    printf("Invalid motor index %d\n", motor_index);
    return false;
}

def_prim(drive_motor_ms, threeToNoneU32) {
    int32_t time = arg0.uint32;
    int32_t speed = arg1.int32;
    int32_t motor_index = arg2.int32;
    printf("drive_motor_ms(%d, %d, %d)\n", motor_index, speed, time);
    pop_args(3);

    if (auto motor = get_motor(motor_index)) {
        motor.value().set_speed(speed / 10000.0f);
        k_msleep(time);
        motor.value().halt();
        return true;
    }

    printf("Invalid motor index %d\n", motor_index);
    return false;
}

bool drive_motor_degrees_absolute(uint32_t motor_index, int32_t degrees,
                                  int32_t speed) {
    if (auto motor = get_motor(motor_index)) {
        motor->drive_to_angle(speed, degrees);
        return true;
    }
    return false;
}

bool drive_motor_degrees_relative(uint32_t motor_index, int32_t degrees,
                                  int32_t speed) {
    MotorEncoder *encoder = encoders[motor_index];
    return drive_motor_degrees_absolute(
        motor_index, encoder->get_target_angle() + degrees, speed);
}

def_prim(drive_motor_degrees, threeToNoneU32) {
    int32_t degrees = arg0.int32;
    int32_t speed = arg1.int32;
    uint32_t motor_index = arg2.uint32;
    pop_args(3);
    return drive_motor_degrees_relative(motor_index, degrees, speed);
}

def_prim_reverse(drive_motor_degrees) {
    for (IOStateElement state : external_state) {
        if (!state.output) {
            continue;
        }

        if (state.key[0] == 'e') {
            printf("Motor target location %d\n", state.value);
            int motor_index = stoi(state.key.substr(1));
            // TODO: This is a hack, we should take snapshots before calling
            // primitives instead of after and just not restore io when
            // restoring the last snapshot and transfer overrides from a future
            // snapshot when doing forward execution.
            drive_motor_degrees_absolute(motor_index, (int32_t)state.value,
                                         motor_index == 0 ? 10000 : 2000);
        }
    }
}

def_prim_serialize(drive_motor_degrees) {
    for (int i = 0; i < 2; i++) {
        IOStateElement *state = new IOStateElement();
        state->output = true;
        state->key = "e" + std::to_string(i);
        state->value = encoders[i]->get_target_angle();
        external_state.push_back(state);
    }
}

#define UART_ENTRY(idx, _) \
    DEVICE_DT_GET(DT_PHANDLE_BY_IDX(DT_PATH(zephyr_user), warduino_uarts, idx))
const device *const uart_devs[] = {LISTIFY(
    DT_PROP_LEN(DT_PATH(zephyr_user), warduino_uarts), UART_ENTRY, (, ))};

UartSensor sensors[] = {UartSensor(uart_devs[0]), UartSensor(uart_devs[1]),
                        UartSensor(uart_devs[2]), UartSensor(uart_devs[3])};

def_prim(setup_uart_sensor, twoToNoneU32) {
    printf("get sensor %d\n", arg1.uint32);
    UartSensor *sensor = &sensors[arg1.uint32];
    bool result = configure_uart_sensor(sensor, arg0.uint32);
    pop_args(2);
    return result;
}

def_prim(read_uart_sensor, oneToOneI32) {
    printf("read_uart_sensor(%d)\n", arg0.uint32);
    UartSensor *sensor = &sensors[arg0.uint32];
    if (!sensor_ready(sensor)) {
        printk("Input port is not ready!\n");
        return false;
    }

    pop_args(1);
    int32_t value = get_sensor_value(sensor);
    pushInt32(value);
    return true;
}

struct k_timer heartbeat_timer;
void heartbeat_timer_func(struct k_timer *timer_id) {
    for (int i = 0; i < 4; i++) {
        uartHeartbeat(&sensors[i]);
    }
}
#endif

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives() {
    dbg_info("INSTALLING PRIMITIVES\n");
    install_primitive(chip_delay);
    install_primitive(chip_pin_mode);
    install_primitive(chip_digital_write);
    install_primitive_reverse(chip_digital_write);
    install_primitive(chip_digital_read);
    install_primitive(chip_analog_read);
    install_primitive(print_int);
    install_primitive(abort);

#ifdef CONFIG_BOARD_STM32L496G_DISCO
    install_primitive(drive_motor);
    install_primitive(stop_motor);
    install_primitive(drive_motor_ms);
    install_primitive(drive_motor_degrees);
    install_primitive_reverse(drive_motor_degrees);

    install_primitive(read_uart_sensor);
    install_primitive(setup_uart_sensor);

    install_primitive(ev3_touch_sensor);

    k_timer_init(&heartbeat_timer, heartbeat_timer_func, nullptr);
    k_timer_start(&heartbeat_timer, K_MSEC(500), K_MSEC(500));
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
    std::set<std::string> prim_names;
    for (uint32_t i = 0; i < m->import_count; i++) {
        prim_names.emplace(m->functions[i].import_field);
    }

    for (PrimitiveEntry &p : primitives) {
        if (prim_names.find(p.name) != prim_names.end()) {
            printf("%s\n", p.name);
            if (p.f_reverse) {
                printf("Reversing action for primitive %s\n", p.name);
                p.f_reverse(m, external_state);
            }
        }
    }
}

std::vector<IOStateElement *> get_io_state(Module *) {
    std::vector<IOStateElement *> ioState;
    for (auto &primitive : primitives) {
        if (primitive.f_serialize_state) {
            primitive.f_serialize_state(ioState);
        }
    }
    return ioState;
}

#endif  // ARDUINO
