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
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
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

def_prim(millis, NoneToOneU32) {
    pushInt32(k_uptime_get());
    return true;
}

def_prim(random_int, NoneToOneU32) {
    pushInt32(sys_rand32_get());
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

int16_t read_value(int c, int *channels) {
    int16_t buf = -1;
    int channel = channels[c];
    if (channel < 0) {
        printf("Invalid channel for chip_analog_read(%d)\n", channel);
        return -1;
    }
    struct adc_sequence seq = {.channels = BIT(channel),
                               .buffer = &buf,
                               .buffer_size = sizeof(buf),
                               .resolution = 12};

    const device *adc_dev;
    adc_channel_cfg cfg;

    // Port 0 uses adc3, the other ports use adc1.
    if (c == 0) {
        adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc3));
        struct adc_channel_cfg channel_cfgs[] = {
            DT_FOREACH_CHILD_SEP(DT_NODELABEL(adc3), ADC_CHANNEL_CFG_DT, (, ))};
        cfg = channel_cfgs[channel];
    } else {
        adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc1));
        struct adc_channel_cfg channel_cfgs[] = {
            DT_FOREACH_CHILD_SEP(DT_NODELABEL(adc1), ADC_CHANNEL_CFG_DT, (, ))};
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
        return -1;
    }

    printf("ADC read value(channel = %d): %d\n", channel, buf);
    return buf;
}

def_prim(nxt_touch_sensor, oneToOneU32) {
    int channels[4] = {
        12,  // Port 1
        6,   // Port 2
        7,   // Port 3
        8,   // Port 4
    };
    int16_t v = read_value(arg0.uint32, channels);
    pop_args(1);
    pushUInt32(v < 2000);
    return true;
}

// Currently only works on ports 1, 3, and 4.
def_prim(ev3_touch_sensor, oneToOneU32) {
    printf("ev3_touch_sensor(%u)\n", arg0.uint32);

    int channels[4] = {
        13,  // Port 1
        5,   // Port 2
        9,   // Port 3
        10,  // Port 4
    };
    int16_t v = read_value(arg0.uint32, channels);
    pop_args(1);
    pushUInt32(v > 3000);
    return true;
}

#endif

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_display)) && IS_ENABLED(CONFIG_DISPLAY)
void draw_rect(const device *display_dev, int xpos, int ypos, int w, int h,
               uint16_t color) {
    struct display_buffer_descriptor new_buf_desc;
    new_buf_desc.width = w;
    new_buf_desc.height = h;
    new_buf_desc.buf_size = new_buf_desc.width * new_buf_desc.height;
    new_buf_desc.pitch = new_buf_desc.width;
    new_buf_desc.frame_incomplete = false;
    uint16_t *new_buf = static_cast<uint16_t *>(
        malloc(sizeof(uint16_t) * new_buf_desc.buf_size));  // 16 bit color
    for (int y = 0; y < new_buf_desc.height; y++) {
        for (int x = 0; x < new_buf_desc.width; x++) {
            new_buf[y * new_buf_desc.width + x] = color;
        }
    }
    display_write(display_dev, xpos, ypos, &new_buf_desc, new_buf);
    free(new_buf);
}

#include "vgafont.h"

void draw_char(const device *display_dev, int xpos, int ypos, char c,
               uint16_t foreground, uint16_t background) {
    struct display_buffer_descriptor new_buf_desc;
    new_buf_desc.width = 8;
    new_buf_desc.height = 16;
    new_buf_desc.buf_size = new_buf_desc.width * new_buf_desc.height;
    new_buf_desc.pitch = new_buf_desc.width;
    new_buf_desc.frame_incomplete = false;
    uint16_t *new_buf = static_cast<uint16_t *>(
        malloc(sizeof(uint16_t) * new_buf_desc.buf_size));  // 16 bit color
    for (int y = 0; y < new_buf_desc.height; y++) {
        for (int x = 0; x < new_buf_desc.width; x++) {
            new_buf[y * new_buf_desc.width + x] =
                (font16[16 * c + y] >> (8 - x)) & 0b1 == 1 ? foreground
                                                           : background;
        }
    }
    display_write(display_dev, xpos, ypos, &new_buf_desc, new_buf);
    free(new_buf);
}

const device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

def_prim(display_setup, NoneToNoneU32) {
    struct display_capabilities capabilities;

    if (!device_is_ready(display_dev)) {
        printf("Device %s not found. Aborting sample.", display_dev->name);
        return false;
    }

    printf("Display setup for %s ", display_dev->name);
    display_get_capabilities(display_dev, &capabilities);

    printf("w = %d, h = %d\n", capabilities.x_resolution,
           capabilities.y_resolution);
    printf("Display color format %d\n", capabilities.current_pixel_format);

    // Fill display in steps so we don't fill up memory with a huge framebuffer.
    for (int x = 0; x < capabilities.x_resolution; x++) {
        draw_rect(display_dev, x, 0, 1, capabilities.y_resolution, 0x0000);
    }
    // display_clear(display_dev);
    display_blanking_off(display_dev);
    return true;
}

def_prim(display_width, NoneToOneU32) {
    display_capabilities capabilities;
    display_get_capabilities(display_dev, &capabilities);
    pushUInt32(capabilities.x_resolution);
    return true;
}

def_prim(display_height, NoneToOneU32) {
    display_capabilities capabilities;
    display_get_capabilities(display_dev, &capabilities);
    pushUInt32(capabilities.y_resolution);
    return true;
}

def_prim(display_fill_rect, fiveToNoneU32) {
    draw_rect(display_dev, arg4.int32, arg3.int32, arg2.int32, arg1.int32,
              arg0.uint32);
    pop_args(5);
    return true;
}

def_prim(display_draw_string, sixToNoneU32) {
    uint32_t addr = arg3.uint32;
    uint32_t size = arg2.uint32;
    std::string text = parse_utf8_string(m->memory.bytes, size, addr);
    for (int i = 0; i < text.length(); i++) {
        draw_char(display_dev, arg5.int32 + i * 8, arg4.int32, text[i],
                  arg1.uint32, arg0.uint32);
    }
    pop_args(6);
    return true;
}
#endif

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives(Interpreter *interpreter) {
    dbg_info("INSTALLING PRIMITIVES\n");
    install_primitive(chip_delay);
    install_primitive(chip_pin_mode);
    install_reversible_primitive(chip_digital_write);
    install_primitive(chip_digital_read);
    install_primitive(millis);
    install_primitive(random_int);
    install_primitive(print_string);
    install_primitive(print_int);
    install_primitive(abort);

#ifdef CONFIG_BOARD_STM32L496G_DISCO
    install_primitive(drive_motor);
    install_primitive(stop_motor);
    install_primitive(drive_motor_ms);
    install_reversible_primitive(drive_motor_degrees);

    install_primitive(read_uart_sensor);
    install_primitive(setup_uart_sensor);

    install_primitive(nxt_touch_sensor);
    install_primitive(ev3_touch_sensor);

    k_timer_init(&heartbeat_timer, heartbeat_timer_func, nullptr);
    k_timer_start(&heartbeat_timer, K_MSEC(500), K_MSEC(500));
#endif

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_display)) && IS_ENABLED(CONFIG_DISPLAY)
    install_primitive(display_setup);
    install_primitive(display_width);
    install_primitive(display_height);
    install_primitive(display_fill_rect);
    install_primitive(display_draw_string);
#endif
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
