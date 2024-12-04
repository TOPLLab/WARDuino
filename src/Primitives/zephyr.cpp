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
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/devicetree/gpio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "primitives.h"
#include "Mindstorms/Motor.h"
#include "Mindstorms/uart_sensor.h"

#define ALL_PRIMITIVES 11

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
            p->f_reverse = nullptr;                                        \
            p->f_serialize_state = nullptr;                                \
        } else {                                                           \
            FATAL("pim_index out of bounds");                              \
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

/*static const struct pwm_dt_spec pwm_led0 =
PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0); static const struct pwm_dt_spec
pwm_led1 = PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);*/
static const struct pwm_dt_spec pwm_led0 =
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2);
static const struct pwm_dt_spec pwm_led1 =
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 3);

// TODO: Use Zephyr FOREACH macro here
const struct pwm_dt_spec pwm_specs[] = {
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0),
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1),
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 2),
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 3),
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 4),
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 5),
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 6),
    PWM_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 7),
};

/*struct gpio_dt_spec pwm_specs[] = {
        DT_FOREACH_PROP_ELEM_SEP(DT_PATH(zephyr_user), pmws,
                                 PWM_DT_SPEC_GET_BY_IDX, (,))
};*/

def_prim(chip_pin_mode, twoToNoneU32) {
    gpio_dt_spec pin_spec = specs[arg1.uint32];
    gpio_pin_configure(pin_spec.port, pin_spec.pin,
                       arg0.uint32 == 0 ? GPIO_INPUT : GPIO_OUTPUT);
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

def_prim(print_int, oneToNoneU32) {
    printf("%u\n", arg0.uint32);
    pop_args(1);
    return true;
}

MotorEncoder *encoders[] = {new MotorEncoder(specs[51], specs[50], "Port A"),
                            new MotorEncoder(specs[57], specs[58], "Port B"),
                            new MotorEncoder(specs[17], specs[13], "Port C"),
                            new MotorEncoder(specs[27], specs[26], "Port D")};

std::optional<Motor> get_motor(int32_t motor_index) {
    if (motor_index > 3) {
        return {};
    }

    return std::make_optional<Motor>(pwm_specs[motor_index * 2], pwm_specs[motor_index * 2 + 1], encoders[motor_index]);
}

def_prim(drive_motor, threeToNoneU32) {
    int32_t brake = arg0.int32;
    int32_t speed = arg1.int32;
    int32_t motor_index = arg2.int32;

    printf("drive_motor(%d, %d, %d)\n", motor_index, speed, brake);

    if (motor_index > 3) {
        printf("Invalid motor index %d\n", motor_index);
        pop_args(3);
        return true;
    }

    Motor motor = get_motor(motor_index).value();
    motor.set_speed(speed / 10000.0f);

    if (speed == 0 && brake == 1) {
        motor.halt();
    }

    pop_args(3);
    return true;
}

def_prim(drive_motor_ms, twoToNoneU32) {
    int32_t motor_index = arg1.int32;
    int32_t speed = (int32_t)arg0.uint32;
    printf("drive_motor_ms(%d, %d)\n", motor_index, speed);

    Motor motor = get_motor(motor_index).value();
    motor.set_speed(speed / 10000.0f);
    k_msleep(arg1.uint32);
    motor.halt();
    pop_args(2);
    return true;
}

bool drive_motor_degrees_absolute(int32_t motor_index, int32_t degrees, int32_t speed) {
    if (auto motor = get_motor(motor_index)) {
        motor->drive_to_angle(speed, degrees);
        return true;
    }
    return false;
}

bool drive_motor_degrees_relative(int32_t motor_index, int32_t degrees, int32_t speed) {
    MotorEncoder *encoder = encoders[motor_index];
    drive_motor_degrees_absolute(motor_index, encoder->get_target_angle() + degrees, speed);
    return true;
}

def_prim(drive_motor_degrees, threeToNoneU32) {
    int32_t speed = arg0.int32;
    int32_t degrees = arg1.uint32;
    int32_t motor_index = arg2.uint32;
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
            drive_motor_degrees_absolute(motor_index, (int32_t) state.value, motor_index == 0 ? 10000 : 2000);
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

extern void read_debug_messages();

void debug_work_handler(struct k_work *work) {
    read_debug_messages();
}

K_WORK_DEFINE(debug_work, debug_work_handler);

struct k_timer heartbeat_timer;
void heartbeat_timer_func(struct k_timer *timer_id) {
    uartHeartbeat();
    k_work_submit(&debug_work);
}

def_prim(setup_uart_sensor, twoToNoneU32) {
    bool result = configure_uart_sensor(arg0.uint32);
    pop_args(2);
    return result;
}

def_prim(color_sensor, oneToOneU32) {
    if (!sensor_ready()) {
        printk("Input port is not ready!\n");
        return false;
    }

    pop_args(1);
    int sensor_value = get_sensor_value();
    if (sensor_value < 0) sensor_value = 0;
    if (sensor_value > 7) sensor_value = 7;
    pushUInt32(sensor_value);
    return true;
}

def_prim(abort, NoneToNoneU32) {
    printf("abort\n");
    return false;
}

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
    install_primitive(print_int);

    install_primitive(drive_motor);
    install_primitive(drive_motor_ms);
    install_primitive(drive_motor_degrees);
    install_primitive_reverse(drive_motor_degrees);

    install_primitive(color_sensor);
    install_primitive(setup_uart_sensor);

    install_primitive(abort);

    k_timer_init(&heartbeat_timer, heartbeat_timer_func, nullptr);
    k_timer_start(&heartbeat_timer, K_MSEC(990), K_MSEC(990));
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

std::vector<IOStateElement *> get_io_state(Module *m) {
    std::vector<IOStateElement *> ioState;
    for (auto &primitive : primitives) {
        if (primitive.f_serialize_state) {
            primitive.f_serialize_state(ioState);
        }
    }
    return ioState;
}

#endif  // ARDUINO
