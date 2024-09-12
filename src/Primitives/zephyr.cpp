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
#include <zephyr/drivers/uart.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/kernel.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

#include "../IO/io.h"
#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "primitives.h"

#define NUM_PRIMITIVES 0
#define NUM_PRIMITIVES_ARDUINO 11

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

bool drive_pwm(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, float pwm1,
               float pwm2) {
    if (!pwm_is_ready_dt(&pwm1_spec)) {
        printf("Error: PWM device %s is not ready\n", pwm1_spec.dev->name);
        return false;
    }

    int ret = pwm_set_pulse_dt(&pwm1_spec, pwm1 * pwm1_spec.period);
    if (ret) {
        printf("Error %d: failed to set pulse width, pwm1 = %f\n", ret, pwm1);
        return false;
    }

    ret = pwm_set_pulse_dt(&pwm2_spec, pwm2 * pwm2_spec.period);
    if (ret) {
        printf("Error %d: failed to set pulse width, pwm2 = %f\n", ret, pwm2);
        return false;
    }
    return true;
}

bool drive_motor(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, float speed) {
    float pwm1 = 0;
    float pwm2 = 0;
    if (speed > 0) {
        pwm1 = speed;
    } else {
        pwm2 = -speed;
    }

    return drive_pwm(pwm1_spec, pwm2_spec, pwm1, pwm2);
}

def_prim(motor_test, oneToNoneU32) {
    int32_t speed = (int32_t)arg0.uint32;
    printf("motor_test(%d)\n", speed);
    drive_motor(pwm_led0, pwm_led1, speed / 10000.0f);
    pop_args(1);
    return true;
}

def_prim(drive_motor_for_ms, twoToNoneU32) {
    int32_t speed = (int32_t)arg0.uint32;
    printf("drive_motor_for_ms(%d, %d)\n", speed, arg1.uint32);

    drive_motor(pwm_led0, pwm_led1, speed / 10000.0f);
    k_msleep(arg1.uint32);
    drive_pwm(pwm_led0, pwm_led1, 1.0f, 1.0f);
    pop_args(2);
    return true;
}

class MotorEncoder {
    static void encoder_pin5_edge_rising(const struct device *dev,
                                         struct gpio_callback *cb,
                                         uint32_t pins) {
        MotorEncoder *encoder =
            CONTAINER_OF(cb, MotorEncoder, pin5_encoder_cb_data);
        if (!encoder->expect_pin5_int) return;

        if (!gpio_pin_get_raw(encoder->pin6_encoder_spec.port,
                              encoder->pin6_encoder_spec.pin)) {
            encoder->angle++;
        } else {
            encoder->angle--;
        }
        encoder->last_update = k_uptime_get();
        // printk("Rising edge detected on encoder pin5, angle %d\n",
        // encoder->angle); printk("%d\n",
        // gpio_pin_get_raw(encoder->pin6_encoder_spec.port,
        // encoder->pin6_encoder_spec.pin));
        encoder->expect_pin5_int = false;
        encoder->expect_pin6_int = true;
    }

    static void encoder_pin6_edge_rising(const struct device *dev,
                                         struct gpio_callback *cb,
                                         uint32_t pins) {
        MotorEncoder *encoder =
            CONTAINER_OF(cb, MotorEncoder, pin6_encoder_cb_data);
        if (!encoder->expect_pin6_int) return;

        if (gpio_pin_get_raw(encoder->pin5_encoder_spec.port,
                             encoder->pin5_encoder_spec.pin)) {
            encoder->angle++;
        } else {
            encoder->angle--;
        }
        // printk("Rising edge detected on encoder pin6, angle %d\n",
        // encoder->angle); printk("%d\n",
        // gpio_pin_get_raw(encoder->pin5_encoder_spec.port,
        // encoder->pin5_encoder_spec.pin));
        encoder->expect_pin6_int = false;
        encoder->expect_pin5_int = true;
    }

   public:
    MotorEncoder(gpio_dt_spec pin5_encoder_spec, gpio_dt_spec pin6_encoder_spec)
        : pin5_encoder_spec(pin5_encoder_spec),
          pin6_encoder_spec(pin6_encoder_spec),
          angle(0),
          target_angle(0),
          expect_pin5_int(true),
          expect_pin6_int(true),
          last_update(0) {
        if (gpio_pin_configure_dt(&pin5_encoder_spec, GPIO_INPUT)) {
            FATAL("Failed to configure GPIO encoder pin5\n");
        }
        if (gpio_pin_configure_dt(&pin6_encoder_spec, GPIO_INPUT)) {
            FATAL("Failed to configure GPIO encoder pin6\n");
        }

        gpio_pin_interrupt_configure_dt(&pin5_encoder_spec,
                                        GPIO_INT_EDGE_RISING);
        gpio_init_callback(&pin5_encoder_cb_data,
                           MotorEncoder::encoder_pin5_edge_rising,
                           BIT(pin5_encoder_spec.pin));
        gpio_add_callback(pin5_encoder_spec.port, &pin5_encoder_cb_data);

        gpio_pin_interrupt_configure_dt(&pin6_encoder_spec,
                                        GPIO_INT_EDGE_RISING);
        gpio_init_callback(&pin6_encoder_cb_data,
                           MotorEncoder::encoder_pin6_edge_rising,
                           BIT(pin6_encoder_spec.pin));
        gpio_add_callback(pin6_encoder_spec.port, &pin6_encoder_cb_data);
    }

    ~MotorEncoder() {
        gpio_remove_callback(pin5_encoder_spec.port, &pin5_encoder_cb_data);
        gpio_remove_callback(pin6_encoder_spec.port, &pin6_encoder_cb_data);
    }

    int get_angle() { return angle; }

    void reset_angle() { angle = 0; }

    int get_target_angle() { return target_angle; }

    void set_target_angle(int target_angle) {
        this->target_angle = target_angle;
    }

    int64_t get_last_update() { return last_update; }

   private:
    gpio_dt_spec pin5_encoder_spec;
    gpio_dt_spec pin6_encoder_spec;
    struct gpio_callback pin5_encoder_cb_data;
    struct gpio_callback pin6_encoder_cb_data;
    volatile int angle;
    int target_angle;
    bool expect_pin5_int;
    bool expect_pin6_int;

   public:
    volatile int64_t last_update;
};

// MotorEncoder encoder(specs[51], specs[50]);
// MotorEncoder encoder(specs[57], specs[58]);
// MotorEncoder encoder(specs[58], specs[57]);

MotorEncoder encoders[] = {MotorEncoder(specs[51], specs[50]),
                           MotorEncoder(specs[57], specs[58])};
// MotorEncoder test_encoder = MotorEncoder(specs[57], specs[58]);

void drive_motor_to_target(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, MotorEncoder *encoder, int32_t speed) {
    printf("drift = %d\n",
           abs(encoder->get_angle() - encoder->get_target_angle()));

    int drift = encoder->get_angle() - encoder->get_target_angle();
    // Reset stall timer, otherwise it will instantly think it's not moving.
    encoder->last_update = k_uptime_get();
    while (abs(drift) > 0) {
        int speed_sign = std::signbit(drift) ? -1 : 1;
        drive_motor(pwm1_spec, pwm2_spec, speed_sign * speed / 10000.0f);
        while (speed_sign *
                       (encoder->get_angle() - encoder->get_target_angle()) >
                   0 &&
               k_uptime_get() - encoder->get_last_update() < 150) {
        }
        bool not_moving = k_uptime_get() - encoder->get_last_update() >= 150;
        if (not_moving) {
            speed += 100;
            printf("Not moving, increasing speed to %d, %llims since last movement\n", speed, k_uptime_get() - encoder->get_last_update());
            drive_motor(pwm1_spec, pwm2_spec, speed_sign * speed / 10000.0f);

            // Wait for 10ms or movement.
            uint64_t start_time = k_uptime_get();
            while (k_uptime_get() - start_time < 10 && k_uptime_get() - encoder->get_last_update() >= 150) {}
            continue;
        }
        encoder->last_update = k_uptime_get();
        printf("%lli\n", k_uptime_get() - encoder->get_last_update());
        /*printf("PWM device %s\n", pwm1_spec.dev->name);
        printf("PWM device %s\n", pwm2_spec.dev->name);*/
        drive_pwm(pwm1_spec, pwm2_spec, 1.0f, 1.0f);
        k_msleep(50);
        drift = encoder->get_angle() - encoder->get_target_angle();
        printf("drift = %d, speed = %d\n", drift, speed);
        // speed = std::max(775, speed - speed/3); // Reduce speed when going
        // fast to do corrections speed = 775;
        if (not_moving) {
            speed += 100;
        } else {
            speed = 800;
        }
    }
}

bool drive_motor_degrees_relative(int32_t motor_index, int32_t degrees, int32_t speed) {
    printf("drive_motor_degrees(%d, %d, %d)\n", motor_index, degrees, speed);

    if (motor_index > 1) {
        return false;
    }

    pwm_dt_spec pwm1_spec = pwm_specs[motor_index * 2];
    pwm_dt_spec pwm2_spec = pwm_specs[motor_index * 2 + 1];
    MotorEncoder *encoder = &encoders[motor_index];

    encoder->set_target_angle(encoder->get_target_angle() + degrees);

    drive_motor_to_target(pwm1_spec, pwm2_spec, encoder, speed);
    return true;
}

def_prim(drive_motor_degrees, threeToNoneU32) {
    int32_t speed = (int32_t)arg0.uint32;
    //speed = 10000;
    int32_t degrees = (int32_t)arg1.uint32;
    //degrees = 360 * 20;
    int32_t motor_index = (int32_t)arg2.uint32;
    bool result = drive_motor_degrees_relative(motor_index, degrees, speed);
    // Print fidx, arguments and the state associated with this primitive before executing the action.
    //printf("ACTION {")
    pop_args(3);
    return result;
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
            invoke_primitive(m, "drive_motor_degrees_absolute",
                             motor_index, (uint32_t)state.value,
                             motor_index == 0 ? 10000 : 2000);
        }
    }
}

def_prim_serialize(drive_motor_degrees) {
    for (int i = 0; i < 2; i++) {
        IOStateElement *state = new IOStateElement();
        state->output = true;
        state->key = "e" + std::to_string(i);
        state->value = encoders[i].get_target_angle();
        external_state.push_back(state);
    }
}

def_prim(drive_motor_degrees_absolute, threeToNoneU32) {
    int32_t speed = (int32_t)arg0.uint32;
    int32_t degrees = (int32_t)arg1.uint32;
    int32_t motor_index = (int32_t)arg2.uint32;
    printf("drive_motor_degrees_absolute(%d, %d, %d)\n", motor_index, degrees,
           speed);

    if (motor_index > 1) {
        printf("Invalid motor index %d\n", motor_index);
        pop_args(3);
        return true;
    }

    pwm_dt_spec pwm1_spec = pwm_specs[motor_index * 2];
    pwm_dt_spec pwm2_spec = pwm_specs[motor_index * 2 + 1];
    MotorEncoder *encoder = &encoders[motor_index];

    encoder->set_target_angle(degrees);

    drive_motor_to_target(pwm1_spec, pwm2_spec, encoder, speed);

    pop_args(3);
    return true;
}

static const struct device *const uart_dev =
    DEVICE_DT_GET(DT_PROP(DT_PATH(zephyr_user), warduino_uarts));
volatile int payload_bytes = 0;
int payload_index = 0;
unsigned int current_payload = 0;
char checksum;
int baudrate = -1;
int bytes_received = 0;

int mode0_format_location = 0;

volatile bool data_mode = false;
bool data_byte = false;
volatile int sensor_value = 0;

void serial_cb(const struct device *dev, void *user_data) {
    uint8_t data;

    if (!uart_irq_update(uart_dev)) {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev)) {
        return;
    }

    /* read until FIFO empty */
    while (uart_fifo_read(uart_dev, &data, 1) == 1) {
        bytes_received++;
        /*printk("0x%02x '%c' ", data, (char) data);
        printk("0b");
        for (int i = 7; i >= 0; i--) {
            printk("%d", (data & 1 << i) > 0);
        }
        printk("\n");*/

        if (payload_bytes > 0) {
            printk("payload = %d\n", data);
            // Print in binary:
            printk("0b");
            for (int i = 7; i >= 0; i--) {
                printk("%d", (data & 1 << i) > 0);
            }
            printk("\n");
            payload_bytes--;

            if (payload_bytes > 1) {
                checksum ^= data;
                printk("before current_payload = %d\n", current_payload);
                current_payload = current_payload |
                                  (((unsigned long)data) << payload_index * 8);
                payload_index++;
                printk("shift = %d, current_payload = %d\n", payload_index * 8,
                       (int)current_payload);
                // printk("checksum = %d\n", checksum);
            } else if (checksum == data) {
                printk("Checksum matches!\n");
                printk("Baudrate = %d\n", current_payload);
                baudrate = current_payload;  // TODO: Set actual baudrate here
            }
        }

        // If data is ACK send an ACK back.
        /*if (data == 0b00000100) {
            printk("Bytes received = %d\n", bytes_received);
            uart_poll_out(uart_dev, 0b00000100);
            uart_poll_out(uart_dev, 0b00000010);
            printk("Sent ACK\n");

            // If we received a baudrate, change it after sending the ACK.
            if (baudrate >= 0) {
                printk("Changing baudrate to %d\n", baudrate);
                uart_config cfg;
                uart_config_get(uart_dev, &cfg);
                cfg.baudrate = baudrate;

                int config_err = uart_configure(uart_dev, &cfg);
                printk("config_err = %d\n", config_err);
                if (config_err) {
                    printk("UART configure error %d", config_err);
                }

                uart_config_get(uart_dev, &cfg);
                printk("current baudrate after config change = %d\n",
        cfg.baudrate);
            }
        }*/

        // When we receive an ACK message.
        if (data == 0b00000100) {
            // printk("%d\n", bytes_received - mode0_format_location);
            if (bytes_received - mode0_format_location == 7 && baudrate >= 0) {
                printk("SPECIAL_LINE\n");

                uart_poll_out(uart_dev, 0b00000100);  // Send ACK
                data_mode = true;

                // Change baudrate:
                /*printk("Changing baudrate to %d\n", baudrate);
                uart_config cfg;
                uart_config_get(uart_dev, &cfg);
                cfg.baudrate = baudrate;

                int config_err = uart_configure(uart_dev, &cfg);
                printk("config_err = %d\n", config_err);
                if (config_err) {
                    printk("UART configure error %d", config_err);
                }

                config_err = uart_config_get(uart_dev, &cfg);
                printk("current baudrate after config change = %d\n",
                cfg.baudrate); printk("config_err = %d\n", config_err);*/

                /*while (true) {
                    k_msleep(100);
                    printk("Send NACK\n");
                    uart_poll_out(uart_dev, 0b00000010);
                }*/
            }
        }

        // HACK
        if (data == 0b10010000 && baudrate >= 0) {
            mode0_format_location = bytes_received;
            // Receive last bits of data and ACK it:
            /*k_msleep(50);
            uart_poll_out(uart_dev, 0b00000100); // Send ACK

            // Change baudrate:
            printk("Changing baudrate to %d\n", baudrate);
            uart_config cfg;
            uart_config_get(uart_dev, &cfg);
            cfg.baudrate = baudrate;

            int config_err = uart_configure(uart_dev, &cfg);
            printk("config_err = %d\n", config_err);
            if (config_err) {
                printk("UART configure error %d", config_err);
            }

            uart_config_get(uart_dev, &cfg);
            printk("current baudrate after config change = %d\n",
            cfg.baudrate);*/
        }

        if (data_mode) {
            if (data_byte) {
                sensor_value = data;
                data_byte = false;
            }

            // Check if it' a data message.
            if (0b11000000 & data) {
                // Next byte will be data
                data_byte = true;
            }
        }

        if (data == 0b01010010) {
            printk("Speed command\n");
            payload_bytes = 5;
            payload_index = 0;
            current_payload = 0;
            checksum = 0xff ^ 0b01010010;

            // EV3 Colour sensor sent 57600 as it's baudrate
        }
    }
    // uart_poll_out(uart_dev, 0b00000100);
    // uart_poll_out(uart_dev, 0b00000010);
    // printk("Sent ACK\n");
}


bool baudrate_configured = false;

extern void read_debug_messages();

void uartHeartbeat() {
    if (data_mode && !baudrate_configured) {
        printk("Changing baudrate to %d\n", baudrate);
        uart_config cfg;
        uart_config_get(uart_dev, &cfg);
        cfg.baudrate = baudrate;

        int config_err = uart_configure(uart_dev, &cfg);
        printk("config_err = %d\n", config_err);
        if (config_err) {
            printk("UART configure error %d", config_err);
        }

        config_err = uart_config_get(uart_dev, &cfg);
        printk("current baudrate after config change = %d\n", cfg.baudrate);
        printk("config_err = %d\n", config_err);
        baudrate_configured = true;
    }

    if (data_mode && baudrate_configured) {
        //k_msleep(100);
        // printk("Send NACK\n");
        uart_poll_out(uart_dev, 0b00000010);

        // printk("sensor_value = %d\n", sensor_value);

        // This timer can sometimes block other things so let's read in new debug messages:
        //read_debug_messages();
    }
}

void my_work_handler(struct k_work *work) {
    uartHeartbeat();
}

K_WORK_DEFINE(my_work, my_work_handler);

void debug_work_handler(struct k_work *work) {
    read_debug_messages();
}

K_WORK_DEFINE(debug_work, debug_work_handler);

struct k_timer heartbeat_timer;
void my_timer_func(struct k_timer *timer_id) {
    uartHeartbeat();
    //printf("My timer func!\n");
    //k_work_submit(&my_work);
    k_work_submit(&debug_work);
}

def_prim(setup_uart_sensor, oneToNoneU32) {
    if (!device_is_ready(uart_dev)) {
        printk("Input port is not ready!\n");
        return 0;
    }

    printk("Setting up uart\n");
    int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
    if (ret < 0) {
        if (ret == -ENOTSUP) {
            printk("Interrupt-driven UART API support not enabled\n");
        } else if (ret == -ENOSYS) {
            printk("UART device does not support interrupt-driven API\n");
        } else {
            printk("Error setting UART callback: %d\n", ret);
        }
        return 0;
    }
    uart_irq_rx_enable(uart_dev);
    pop_args(1);
    return true;
}

/*#define UART_HEARTBEAT_STACK_SIZE 2048
#define UART_HEARTBEAT_PRIORITY 0

K_THREAD_DEFINE(uart_heartbeat_tid, UART_HEARTBEAT_STACK_SIZE, uartHeartbeat,
                NULL, NULL, NULL, UART_HEARTBEAT_PRIORITY, 0, 0);*/

def_prim(colour_sensor, oneToOneU32) {
    if (!device_is_ready(uart_dev)) {
        printk("Input port is not ready!\n");
        return 0;
    }

    // uint16_t data;
    // int res = uart_poll_in_u16(uart_dev, &data);
    // printk("data = %d, res = %d\n", data, res);
#if 0
    unsigned char data;
    int res = uart_poll_in(uart_dev, &data);
    while (res == 0) {
        printk("data = %d, res = %d\n", (int) data, res);
        for (int i = 7; i >= 0; i--) {
            printk("%d", (data & 1 << i) > 0);
        }
        printk("\n");
        //res = uart_poll_in_u16(uart_dev, &data);
        //k_msleep(100);
        k_msleep(10);
        res = uart_poll_in(uart_dev, &data);
    }
    printk("res = %d\n", res);
    uart_poll_out(uart_dev, 0b00000010);
#endif

#if 0
    printk("Setting up uart\n");
    int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
    if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}
	uart_irq_rx_enable(uart_dev);
#endif

    // Send NACK
    /*uart_poll_out(uart_dev, 0b00000010);
    printk("Send NACK\n");*/

    pop_args(1);
    pushUInt32(sensor_value);
    return true;
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
    install_primitive(motor_test);
    install_primitive(drive_motor_for_ms);
    install_primitive(drive_motor_degrees);
    install_primitive_reverse(drive_motor_degrees);
    install_primitive(drive_motor_degrees_absolute);
    install_primitive(colour_sensor);
    install_primitive(setup_uart_sensor);

    k_timer_init(&heartbeat_timer, my_timer_func, nullptr);
    k_timer_start(&heartbeat_timer, K_MSEC(1000), K_MSEC(1000));
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
                            std::vector<IOStateElement> external_state) {
    /*uint8_t opcode = *m->pc_ptr;
    // TODO: Maybe primitives can also be called using the other call operators, maybe there should just be a function that checks if a certain function is being called?
    if (opcode == 0x10) { // call opcode
        uint8_t *pc_copy = m->pc_ptr + 1;
        uint32_t fidx = read_LEB_32(&pc_copy);
        if (fidx < m->import_count) {
            for (auto &primitive : primitives) {
                if (!strcmp(primitive.name, m->functions[fidx].import_field)) {
                    if (primitive.f_reverse) {
                        printf("Reversing action for primitive %s\n", primitive.name);
                        primitive.f_reverse(m, external_state);
                    }
                    return;
                }
            }
        }
    }*/
    /*for (auto &primitive : primitives) {
        printf("%s\n", primitive.name);
        if (primitive.f_reverse) {
            printf("Reversing action for primitive %s\n", primitive.name);
            primitive.f_reverse(m, external_state);
        }
    }*/

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
