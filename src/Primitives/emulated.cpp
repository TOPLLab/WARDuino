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
#include <thread>

#include <SDL2/SDL.h>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"
#include "../WARDuino/CallbackHandler.h"
#include "primitives.h"

#define NUM_PRIMITIVES 0
#define NUM_PRIMITIVES_ARDUINO 36

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
            FATAL("prim_index out of bounds");                             \
        }                                                                  \
    }

#define install_primitive_reverse(prim_name)             \
    {                                                    \
        PrimitiveEntry *p = &primitives[prim_index - 1]; \
        p->f_reverse = &(prim_name## _reverse);           \
        p->f_serialize_state = &(prim_name## _serialize); \
    }

#define def_prim(function_name, type) \
    Type function_name## _type = type; \
    bool function_name([[maybe_unused]] Module *m)

#define def_prim_reverse(function_name)     \
    void function_name## _reverse(Module *m, \
                                 std::vector<IOStateElement> external_state)

#define def_prim_serialize(function_name) \
    void function_name## _serialize(       \
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

Type fiveToNoneU32 = {
    .form = FUNC,
    .param_count = 5,
    .params = param_I32_arr_len5,
    .result_count = 0,
    .results = nullptr,
    .mask =
        0x80011111 /* 0x800 = no return ; 1 = I32; 1 = I32; 1 = I32; 1 = I32; 1 = I32*/
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
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    unsigned long millis = 1000 * tv.tv_sec + tv.tv_usec;
    pushUInt64(millis);
    return true;
}

def_prim(micros, NoneToOneU64) {
    struct timeval tv {};
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

def_prim(chip_pin_mode, twoToNoneU32) {
    debug("EMU: chip_pin_mode(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(chip_digital_write, twoToNoneU32) {
    printf("EMU: chip_digital_write(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(chip_digital_read, oneToOneU32) {
    uint32_t pin = arg0.uint32;
    pop_args(1);
    
    
    SDL_Event event;
    while( SDL_PollEvent( &event ) ){
        switch( event.type ){
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    //SDL_DestroyWindow(window);
                    SDL_Quit();
                    exit(0);
                }
            break;
        }
    }
    
    const uint8_t* keystate = SDL_GetKeyboardState(nullptr);

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

SDL_Renderer *renderer;
SDL_Window* window = nullptr;
SDL_Surface* screenSurface = nullptr;

SDL_Texture *texture = nullptr;
uint32_t tile_size = 32;
uint32_t tile_count = 16;

const int w = 16 * 32;
const int h = 16 * 32;
uint32_t pixels1[w * h] = {};
uint32_t pixels2[w * h] = {};

def_prim(init_display, NoneToNoneU32) {
    printf("init_display \n");
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Could not initialize sdl2: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow(
                    "WARDuino emulated display",
                    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                    640, 480,
                    SDL_WINDOW_SHOWN
                    );
    if (window == NULL) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return false;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);             
    if (!renderer) {                                                                               
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());                            
        return false;                                                                                  
    }
    
    return true;
}

def_prim(dispose_display, NoneToNoneU32) {
    SDL_DestroyWindow(window);
    SDL_Quit();
    return true;
}

def_prim(draw_rect, fiveToNoneU32) {
    uint32_t r = (arg0.uint32 >> 16)  & 0xff;
    uint32_t g = (arg0.uint32 >> 8) & 0xff;
    uint32_t b = arg0.uint32 & 0xff;
    //printf("(%d, %d) (%d, %d)\n", arg4.int32, arg3.int32, arg2.int32, arg1.int32);
    
    SDL_Rect rect = {arg4.int32, arg3.int32, arg2.int32, arg1.int32}; 
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderFillRect(renderer, &rect);
    
    pop_args(5);
    return true;
}

def_prim(draw_sprite, fiveToNoneU32) {
    SDL_Rect rect = {arg4.int32, arg3.int32, arg2.int32, arg1.int32};
    SDL_RenderFillRect(renderer, &rect);
    uint32_t sprite_id = arg0.uint32;
    pop_args(5);

    const int w = tile_size;
    const int h = tile_size;
    const int x = (sprite_id % tile_count) * w;
    const int y = (sprite_id / tile_count) * h;
    SDL_Rect src_rect = {x, y, w, h};
    SDL_RenderCopy(renderer, texture, &src_rect, &rect);

    return true;
}

def_prim(load_sprite, fourToNoneU32) {
    const uint32_t addr = arg3.uint32;
    const uint32_t size = arg2.uint32;
    tile_size = arg1.uint32;
    tile_count = arg0.uint32;
    const std::string filename = parse_utf8_string(m->memory.bytes, size, addr);
    printf("Loading sprite sheet \"%s\" (tilesize %d, grid of %d x %d tiles)\n", filename.c_str(), tile_size, tile_count, tile_count);
    pop_args(2);

    SDL_Surface *texSurface = SDL_LoadBMP(filename.c_str());
    if (!texSurface) {
        char buffer[100];
        snprintf(buffer, 100, "Failed to load sprite sheet \"%s\"", filename.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to load sprite", buffer, window);
        FATAL("Failed to load sprite sheet \"%s\"", filename.c_str());
    }
    texture = SDL_CreateTextureFromSurface(renderer, texSurface);

    return true;
}

def_prim(draw_raw, fiveToNoneU32) {
    uint32_t r = (arg0.uint32 >> 16)  & 0xff;
    uint32_t g = (arg0.uint32 >> 8) & 0xff;
    uint32_t b = arg0.uint32 & 0xff;
    //printf("(%d, %d) (%d, %d)\n", arg4.int32, arg3.int32, arg2.int32, arg1.int32);
    
    SDL_Rect rect = {arg4.int32, arg3.int32, arg2.int32, arg1.int32}; 
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
    
    SDL_Rect src_rect = {0, 0, w, h};
    SDL_RenderCopy(renderer, texture, &src_rect, &rect);
    
    /*int w = 32;
    int h = 32;
    int x = (arg0.uint32 % 16) * w;
    int y = (arg0.uint32 / 16) * h;
    SDL_Rect src_rect = {x, y, w, h};
    SDL_RenderCopy(renderer, texture, &src_rect, &rect);*/
    
    pop_args(5);
    return true;
}

def_prim(present_display_buffer, NoneToNoneU32) {
    SDL_RenderPresent(renderer);
    return true;
}

//------------------------------------------------------
// Installing all the primitives
//------------------------------------------------------
void install_primitives() {
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
    install_primitive(chip_digital_write);
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

    install_primitive(init_display);
    install_primitive(dispose_display); 
    install_primitive(present_display_buffer);
    install_primitive(draw_rect);
    install_primitive(draw_sprite);
    install_primitive(load_sprite);
    install_primitive(draw_raw);
}

//------------------------------------------------------
// resolving the primitives
//------------------------------------------------------
// ReSharper disable once CppDFAConstantFunctionResult
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
    // return false; // unreachable
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
