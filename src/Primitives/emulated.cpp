#include <complex>
#include <cstdint>

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

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_image/SDL_image.h>

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

/*
 * Read an ascii string from memory stored in UTF-16.
 */
std::string read_ascii_utf16(uint8_t *buffer) {
    std::string str = "";
    uint32_t i = 0;
    while (buffer[i] != '\0') {
        str += buffer[i];
        i += 2;
    }
    return str;
}

def_prim(abort, NoneToNoneU32) {
    debug("EMU: abort\n");
 
    std::string message = read_ascii_utf16(&m->memory.bytes[arg3.uint32]);
    std::string filename = read_ascii_utf16(&m->memory.bytes[arg2.uint32]);
    
    printf("\u001b[1m%s:%d:%d:\u001b[0m \u001b[31;1merror:\u001b[0m %s\n", filename.c_str(), arg1.uint32, arg0.uint32, message.c_str());
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

SDL_Renderer *renderer;
SDL_Window* window = nullptr;
float display_scale = 1.0f;

def_prim(chip_digital_read, oneToOneU32) {
    uint32_t pin = arg0.uint32;
    pop_args(1);

    /*if (pin < NUM_DIGITAL_PINS) {
        pushUInt32(PINS[pin]);
        return true;
    }
    return false;*/

    // Input provided by SDL button presses:
    SDL_Event event;
    while( SDL_PollEvent( &event ) ){
        switch( event.type ){
            case SDL_EVENT_QUIT:
                //SDL_DestroyWindow(window);
                SDL_Quit();
                exit(0);
            break;
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                display_scale = SDL_GetWindowDisplayScale(window);
            break;
        }
    }
    
    const bool* keystate = SDL_GetKeyboardState(nullptr);

    //continuous-response keys
    /*if(keystate[SDL_SCANCODE_LEFT] && pin == 1024) {
        pushUInt32(1);
    }
    else if(keystate[SDL_SCANCODE_RIGHT] && pin == 1025) {
        pushUInt32(1);
    }
    else if(keystate[SDL_SCANCODE_UP] && pin == 1026) {
        pushUInt32(1);
    }
    else if(keystate[SDL_SCANCODE_DOWN] && pin == 1027) {
        pushUInt32(1);
    }*/
    if(pin < 128 && keystate[pin]) {
        //printf("%d pressed\n", pin);
        pushUInt32(1);
    }
    else {
        pushUInt32(0);
    }   
    
    return true;
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

struct SpriteSheet {
    SDL_Texture *texture = nullptr;
    uint32_t tile_size = 32;
    uint32_t tile_count = 0;
};

SpriteSheet *spriteSheets = new SpriteSheet[8];
int spriteSheetCount = -1;

const int w = 16 * 32;
const int h = 16 * 32;
uint32_t pixels1[w * h] = {};
uint32_t pixels2[w * h] = {};

struct Font {
    TTF_Font *font = nullptr;
    uint32_t size = 0;
};
std::vector<Font> fonts = {};

MIX_Mixer *mixer;

def_prim(create_window, threeToNoneU32) {
    printf("create_window %d %d %d\n", arg2.uint32, arg1.uint32, arg0.uint32);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "Could not initialize sdl2: %s\n", SDL_GetError());
        return false;
    }
    uint32_t resizable_flag = arg0.uint32 > 0 ? SDL_WINDOW_RESIZABLE : 0;
    window = SDL_CreateWindow(
        "WARDuino emulated display",
        arg2.uint32, arg1.uint32,
        SDL_WINDOW_HIGH_PIXEL_DENSITY | resizable_flag
    );
    if (window == NULL) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return false;
    }
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {                                                                               
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());                            
        return false;                                                                                  
    }
    if (!TTF_Init()) {
        fprintf(stderr, "Failed to init SDL TTF: %s\n", SDL_GetError());
        return false;
    }

    if (!MIX_Init()) {
        fprintf(stderr, "Failed to init SDL Mixer: %s\n", SDL_GetError());
        return false;
    }
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);

    pop_args(3);
    
    return true;
}

def_prim(window_width, NoneToNoneU32) {
    int w;
    SDL_GetWindowSize(window, &w, nullptr);
    pushInt32(w);
    return true;
}

def_prim(window_height, NoneToNoneU32) {
    int h;
    SDL_GetWindowSize(window, nullptr, &h);
    pushInt32(h);
    return true;
}

def_prim(dispose_window, NoneToNoneU32) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return true;
}

def_prim(draw_rect, fiveToNoneU32) {
    uint32_t r = (arg0.uint32 >> 16)  & 0xff;
    uint32_t g = (arg0.uint32 >> 8) & 0xff;
    uint32_t b = arg0.uint32 & 0xff;
    //printf("(%d, %d) (%d, %d)\n", arg4.int32, arg3.int32, arg2.int32, arg1.int32);
    
    SDL_FRect rect = {
        arg4.int32 * display_scale,
        arg3.int32 * display_scale,
        arg2.int32 * display_scale,
        arg1.int32 * display_scale
    };
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderFillRect(renderer, &rect);
    
    pop_args(5);
    return true;
}

void draw_circle_segment(float scaleFactorX, float scaleFactorY, int offsetX, int offsetY, int xpos, int ypos, int cx, int cy) {
    // bottom right
    SDL_FRect rect = {
        (cx + offsetX) * scaleFactorX,
        (cy + offsetY) * scaleFactorY + ypos,
        static_cast<float>(xpos),
        1
    };
    SDL_RenderFillRect(renderer, &rect);
    rect = {
        (cx + offsetX) * scaleFactorX + ypos,
        (cy + offsetY) * scaleFactorY,
        1,
        static_cast<float>(xpos)
    };
    SDL_RenderFillRect(renderer, &rect);

    // bottom left
    rect = {
        cx * scaleFactorX - xpos,
        (cy + offsetY) * scaleFactorY + ypos,
        static_cast<float>(xpos),
        1
    };
    SDL_RenderFillRect(renderer, &rect);
    rect = {
        cx * scaleFactorX - ypos,
        (cy + offsetY) * scaleFactorY,
        1,
        static_cast<float>(xpos)
    };
    SDL_RenderFillRect(renderer, &rect);

    // top part of the circle
    // top right
    rect = {
        (cx + offsetX) * scaleFactorX,
        cy * scaleFactorY - ypos,
        static_cast<float>(xpos),
        1
    };
    SDL_RenderFillRect(renderer, &rect);
    rect = {
        (cx + offsetX) * scaleFactorX + ypos,
        cy * scaleFactorY - xpos,
        1,
        static_cast<float>(xpos)
    };
    SDL_RenderFillRect(renderer, &rect);

    // top left
    rect = {
        cx * scaleFactorX - xpos,
        cy * scaleFactorY - ypos,
        static_cast<float>(xpos),
        1
    };
    SDL_RenderFillRect(renderer, &rect);
    rect = {
        cx * scaleFactorX - ypos,
        cy * scaleFactorY - xpos,
        1,
        static_cast<float>(xpos)
    };
    SDL_RenderFillRect(renderer, &rect);
}

void draw_circle(int32_t cx, int32_t cy, int32_t radius, int32_t color, int32_t spaceX, int32_t spaceY) {
    const uint32_t r = (color >> 16) & 0xff;
    const uint32_t g = (color >> 8) & 0xff;
    const uint32_t b = color & 0xff;
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);

    radius = radius * display_scale;

    int p = 1;
    int r2 = 1;
    for (int i = 0; i < radius - 1; i++) {
        p += 2;
        r2 += p;
    }
    int x = 1;
    int v = r2;
    int xPos=radius, yPos=0;
    draw_circle_segment(display_scale, display_scale, spaceX, spaceY, xPos, yPos, cx, cy);
    while (xPos >= yPos) {
        yPos += 1;
        v += x;
        x += 2;
        if (v > r2) {
            v -= p;
            p -= 2;
            xPos -= 1;
        }
        draw_circle_segment(display_scale, display_scale, spaceX, spaceY, xPos, yPos, cx, cy);
    }
}

def_prim(draw_rounded_rect, sixToNoneU32) {
    int cx = arg5.int32;
    int cy = arg4.int32;
    int radius = arg1.int32;
    int spaceX = arg3.int32 - radius * 2;
    int spaceY = arg2.int32 - radius * 2;
    draw_circle(cx + radius, cy + radius, radius, arg0.uint32, spaceX, spaceY);
    const SDL_FRect hrect = {
        .x = cx * display_scale,
        .y = (cy + radius) * display_scale,
        .w = (spaceX + radius * 2) * display_scale,
        .h = spaceY * display_scale
    };
    SDL_RenderFillRect(renderer, &hrect);
    const SDL_FRect vrect = {
        .x = (cx + radius) * display_scale,
        .y = cy * display_scale,
        .w = spaceX * display_scale,
        .h = (spaceY + radius * 2) * display_scale
    };
    SDL_RenderFillRect(renderer, &vrect);
    pop_args(6);
    return true;
}

def_prim(draw_circle, fourToNoneU32) {
    draw_circle(arg3.int32, arg2.int32, arg1.int32, arg0.uint32, 0, 0);
    pop_args(4);
    return true;
}

def_prim(draw_sprite, sixToNoneU32) {
    SDL_FRect rect = {
        arg5.int32 * display_scale,
        arg4.int32 * display_scale,
        arg3.int32 * display_scale,
        arg2.int32 * display_scale
    };
    SDL_RenderFillRect(renderer, &rect);
    uint32_t sprite_sheet = arg1.uint32;
    uint32_t sprite_id = arg0.uint32;
    pop_args(6);

    SpriteSheet *spriteSheet = &spriteSheets[sprite_sheet];

    const int w = spriteSheet->tile_size;
    const int h = spriteSheet->tile_size;
    const int x = (sprite_id % spriteSheet->tile_count) * w;
    const int y = (sprite_id / spriteSheet->tile_count) * h;
    SDL_FRect src_rect = {
        static_cast<float>(x), 
        static_cast<float>(y), 
        static_cast<float>(w), 
        static_cast<float>(h)
    };
    SDL_RenderTexture(renderer, spriteSheet->texture, &src_rect, &rect);

    return true;
}

def_prim(load_sprite, fourToOneU32) {
    const uint32_t addr = arg3.uint32;
    const uint32_t size = arg2.uint32;
    spriteSheetCount++;
    SpriteSheet *spriteSheet = &spriteSheets[spriteSheetCount];
    spriteSheet->tile_size = arg1.uint32;
    spriteSheet->tile_count = arg0.uint32;
    const std::string filename = parse_utf8_string(m->memory.bytes, size, addr);
    printf("Loading sprite sheet \"%s\" (tilesize %d, grid of %d x %d tiles)\n", filename.c_str(), spriteSheet->tile_size, spriteSheet->tile_count, spriteSheet->tile_count);
    pop_args(4);

    SDL_Surface *texSurface = IMG_Load(filename.c_str());
    if (!texSurface) {
        char buffer[100];
        snprintf(buffer, 100, "Failed to load sprite sheet \"%s\"", filename.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to load sprite", buffer, window);
        FATAL("Failed to load sprite sheet \"%s\"", filename.c_str());
    }
    spriteSheet->texture = SDL_CreateTextureFromSurface(renderer, texSurface);
    SDL_SetTextureScaleMode(spriteSheet->texture, SDL_SCALEMODE_NEAREST);
    pushInt32(spriteSheetCount);
    return true;
}

def_prim(load_font, threeToOneU32) {
    const uint32_t addr = arg2.uint32;
    const uint32_t size = arg1.uint32;
    const uint32_t ptsize = arg0.uint32;
    const std::string filename = parse_utf8_string(m->memory.bytes, size, addr);
    pop_args(3);

    TTF_Font *font = TTF_OpenFont(filename.c_str(), ptsize * display_scale);
    if (!font) {
        char buffer[100];
        snprintf(buffer, 100, "Failed to load font \"%s\"", filename.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to load font", buffer, window);
        FATAL("Failed to load font \"%s\"", filename.c_str());
    }
    pushInt32(fonts.size());
    fonts.push_back(Font { font, ptsize });

    return true;
}

def_prim(set_font_style, twoToNoneU32) {
    TTF_SetFontStyle(fonts[arg1.uint32].font, arg0.int32);
    pop_args(2);
    return true;
}

std::vector<MIX_Audio*> audios = {};
std::vector<MIX_Track*> tracks = {};

def_prim(load_audio, twoToOneU32) {
    const uint32_t addr = arg1.uint32;
    const uint32_t size = arg0.uint32;
    pop_args(2);
    const std::string filename = parse_utf8_string(m->memory.bytes, size, addr);

    // Load and play audio using SDL.
    MIX_Audio *audio = MIX_LoadAudio(mixer, filename.c_str(), false);
    if (!audio) {
        char buffer[100];
        snprintf(buffer, 100, "Failed to load sound \"%s\"", filename.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to load sound", buffer, window);
        FATAL("Failed to load sound \"%s\"", filename.c_str());
    }
    audios.push_back(audio);

    pushInt32(audios.size() - 1);
    return true;
}

def_prim(play_audio, oneToOneU32) {
    const uint32_t audio_idx = arg0.uint32;
    MIX_Audio *audio = audios[audio_idx];

    int track_idx = 0;
    while (track_idx < tracks.size() && MIX_TrackPlaying(tracks[track_idx])) {
        track_idx++;
    }
    if (track_idx >= tracks.size()) {
        MIX_Track *track = MIX_CreateTrack(mixer);
        tracks.push_back(track);
    }

    MIX_Track *track = tracks[track_idx];
    MIX_SetTrackAudio(track, audio);
    if (!MIX_PlayTrack(track, 0)) {
        FATAL("Failed to play audio %s\n", SDL_GetError());
    }
    pop_args(1);
    pushUInt32(track_idx);
    return true;
}

def_prim(get_audio_duration, oneToOneU32) {
    const int audio_idx = arg0.uint32;
    MIX_Audio *audio = audios[audio_idx];
    pop_args(1);
    pushInt64(MIX_AudioFramesToMS(audio, MIX_GetAudioDuration(audio)));
    return true;
}

def_prim(pause_channel, oneToNoneU32) {
    const uint32_t channel = arg0.uint32;
    pop_args(1);
    MIX_PauseTrack(tracks[channel]);
    return true;
}

def_prim(resume_channel, oneToNoneU32) {
    const uint32_t channel = arg0.uint32;
    pop_args(1);
    MIX_ResumeTrack(tracks[channel]);
    return true;
}

def_prim(is_playing_channel, oneToOneU32) {
    const uint32_t channel = arg0.uint32;
    pop_args(1);
    pushInt32(MIX_TrackPlaying(tracks[channel]));
    return true;
}

def_prim(get_channel_position, oneToOneU32) {
    const uint32_t channel = arg0.uint32;
    pop_args(1);
    MIX_Track *track = tracks[channel];
    pushInt64(MIX_TrackFramesToMS(track, MIX_GetTrackPlaybackPosition(track)));
    return true;
}

def_prim(draw_raw, fiveToNoneU32) {
    uint32_t r = (arg0.uint32 >> 16)  & 0xff;
    uint32_t g = (arg0.uint32 >> 8) & 0xff;
    uint32_t b = arg0.uint32 & 0xff;
    //printf("(%d, %d) (%d, %d)\n", arg4.int32, arg3.int32, arg2.int32, arg1.int32);
    
    SDL_FRect rect = {
        static_cast<float>(arg4.int32), 
        static_cast<float>(arg3.int32), 
        static_cast<float>(arg2.int32), 
        static_cast<float>(arg1.int32)
    }; 
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);                                       
    //SDL_RenderDrawRect(renderer, &rect);   
    SDL_RenderFillRect(renderer, &rect);
    
    //SDL_Surface *texSurface = SDL_LoadBMP("/home/maarten/Documents/School/Phd/WARDuinoBuilds/rock.bmp");
    /*char bytes[2 * 2] = {0,1,1,0};
    SDL_Surface *texSurface = SDL_CreateRGBSurface*/
    /*SDL_Surface *texSurface = {
        0,
        50,
        50,
        bytes
    }*/
    
    /*SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        pixels,                        // Pointer to the pixel data
        32, 32,     // Width and height of the image
        8 * 4,                            // Bits per pixel (RGBA = 32)
        32 * 4,               // Pitch (width * 4 bytes per pixel)
        0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000  // Masks for RGBA
    );
    texture = SDL_CreateTextureFromSurface(renderer, surface);*/
    
    int w = arg2.uint32;
    int h = arg1.uint32;
    //printf("addr = %d\n", arg0.uint32);
    for (int i = 0; i < w * h; i++) {
        uint32_t c = m->memory.bytes[arg0.uint32 + i * 4];
        c = c | m->memory.bytes[arg0.uint32 + i * 4 + 1] << 8;
        c = c | m->memory.bytes[arg0.uint32 + i * 4 + 2] << 16;
        c = c | m->memory.bytes[arg0.uint32 + i * 4 + 3] << 24;
        //printf("0x%x ", c);
        
        pixels1[i] = c;
    }
    //printf("\n");
    SDL_Texture *texture = SDL_CreateTexture(renderer, 
                                SDL_PIXELFORMAT_ARGB8888, 
                                SDL_TEXTUREACCESS_STREAMING, 
                                w, h);
    
    SDL_UpdateTexture(texture, NULL, pixels1, w * sizeof(uint32_t));
    
    SDL_FRect src_rect = {0, 0, static_cast<float>(w), static_cast<float>(h)};
    SDL_RenderTexture(renderer, texture, &src_rect, &rect);
    
    /*int w = 32;
    int h = 32;
    int x = (arg0.uint32 % 16) * w;
    int y = (arg0.uint32 / 16) * h;
    SDL_Rect src_rect = {x, y, w, h};
    SDL_RenderCopy(renderer, texture, &src_rect, &rect);*/
    
    pop_args(5);
    return true;
}

def_prim(draw_text, sixToNoneU32) {
    if (arg2.uint32 >= fonts.size()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "No font was loaded", "Please load a font before calling draw_text.", window);
        FATAL("No font was loaded before calling draw_Text\n");
    }

    const Font font = fonts[arg2.uint32];
    const uint32_t addr = arg1.uint32;
    const uint32_t size = arg0.uint32;
    const std::string text = parse_utf8_string(m->memory.bytes, size, addr);
    const uint8_t r = (arg3.uint32 >> 16) & 0xff;
    const uint8_t g = (arg3.uint32 >> 8) & 0xff;
    const uint8_t b = arg3.uint32 & 0xff;
    const SDL_Color color = (SDL_Color) {r, g, b, 0};

    // TODO: TTF_SetFontSize resets the font cache, so this is an expensive operation.
    TTF_SetFontSize(font.font, font.size * display_scale);
    SDL_Surface* surface = TTF_RenderText_Blended(font.font,
        text.c_str(), text.length(), color);
    //assert(surface != nullptr);
    if (surface == nullptr) {
        pop_args(6);
        return true;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    const SDL_FRect rect = {arg5.int32 * display_scale, arg4.int32 * display_scale, static_cast<float>(surface->w), static_cast<float>(surface->h)};
    SDL_RenderTexture(renderer, texture, nullptr, &rect);
    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
    pop_args(6);
    return true;
}

def_prim(text_width, threeToOneU32) {
    const std::string s = parse_utf8_string(m->memory.bytes, arg1.uint32, arg2.uint32);
    const Font font = fonts[arg0.uint32];
    pop_args(3);
    int w;
    TTF_Text *text = TTF_CreateText(nullptr, font.font, s.c_str(), s.length());
    TTF_GetTextSize(text, &w, nullptr);
    TTF_DestroyText(text);
    pushUInt32(w / display_scale);
    return true;
}

def_prim(text_height, threeToOneU32) {
    const std::string s = parse_utf8_string(m->memory.bytes, arg1.uint32, arg2.uint32);
    const Font font = fonts[arg0.uint32];
    pop_args(3);
    int h;
    TTF_Text *text = TTF_CreateText(nullptr, font.font, s.c_str(), s.length());
    TTF_GetTextSize(text, nullptr, &h);
    TTF_DestroyText(text);
    pushUInt32(h / display_scale);
    return true;
}

def_prim(present_display_buffer, NoneToNoneU32) {
    SDL_RenderPresent(renderer);
    return true;
}

def_prim(get_mouse_x, NoneToOneU32) {
    float x, y;
    SDL_GetMouseState(&x, &y);
    pushInt32((int) x);
    return true;
}

def_prim(get_mouse_y, NoneToOneU32) {
    float x, y;
    SDL_GetMouseState(&x, &y);
    pushInt32((int) y);
    return true;
}

def_prim(get_mouse_pressed, NoneToOneU32) {
    pushInt32(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT));
    return true;
}

def_prim(set_window_title, twoToNoneU32) {
    const uint32_t addr = arg1.uint32;
    const uint32_t size = arg0.uint32;
    const std::string window_name = parse_utf8_string(m->memory.bytes, size, addr);
    pop_args(2);

    SDL_SetWindowTitle(window, window_name.c_str());
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

    install_primitive(create_window);
    install_primitive(dispose_window); 
    install_primitive(present_display_buffer);
    install_primitive(window_width);
    install_primitive(window_height);
    install_primitive(draw_rect);
    install_primitive(draw_rounded_rect);
    install_primitive(draw_circle);
    install_primitive(draw_sprite);
    install_primitive(load_sprite);
    install_primitive(draw_raw);
    install_primitive(draw_text);
    install_primitive(load_font);
    install_primitive(set_font_style);
    install_primitive(text_width);
    install_primitive(text_height);

    // Sound primitives
    install_primitive(load_audio);
    install_primitive(play_audio);
    install_primitive(get_audio_duration);
    install_primitive(pause_channel);
    install_primitive(resume_channel);
    install_primitive(is_playing_channel);
    install_primitive(get_channel_position);

    // Mouse primitives
    install_primitive(get_mouse_x);
    install_primitive(get_mouse_y);
    install_primitive(get_mouse_pressed);

    install_primitive(set_window_title);

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
