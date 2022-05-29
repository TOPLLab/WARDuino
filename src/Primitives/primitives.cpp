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

#include <sys/time.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../Memory/mem.h"
#include "../Utils/macros.h"
#include "../Utils/util.h"

#ifdef ARDUINO
#include <HTTPClient.h>
#include <WiFi.h>

#include "Arduino.h"

// NEOPIXEL
#include <Adafruit_NeoPixel.h>
#define PIN 33
#define NUMPIXELS 64
Adafruit_NeoPixel pixels =
    Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

#define delay_us(ms) delayMicroseconds(ms)
#include <SPI.h>
SPIClass *spi = new SPIClass();

// Hardeware SPI
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

#else

#include <chrono>
#include <thread>

#endif

#define NUM_PRIMITIVES 0
#ifdef ARDUINO
#define NUM_PRIMITIVES_ARDUINO 34
#else
#define NUM_PRIMITIVES_ARDUINO 24
#endif

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
        } else {                                                           \
            FATAL("pim_index out of bounds");                              \
        }                                                                  \
    }

#define def_prim(function_name, type) \
    Type function_name##_type = type; \
    bool function_name(Module *m)

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

// Util function declarations
#ifdef ARDUINO

void connect(const String ssid, const String password);

int32_t http_get_request(Module *m, const String url, uint32_t response,
                         uint32_t size);

int32_t http_post_request(Module *m, const String url, const String body,
                          const String content_type,
                          const String authorization_parsed, uint32_t response,
                          uint32_t size);

#endif

//------------------------------------------------------
// Arduino Specific Functions
//------------------------------------------------------
#ifdef ARDUINO

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

def_prim(wifi_connect, fourToNoneU32) {
    uint32_t ssid = arg3.uint32;
    uint32_t len0 = arg2.uint32;
    uint32_t pass = arg1.uint32;
    uint32_t len1 = arg0.uint32;

    String ssid_str = parse_utf8_string(m->memory.bytes, len0, ssid).c_str();
    String pass_str = parse_utf8_string(m->memory.bytes, len1, pass).c_str();

    connect(ssid_str, pass_str);

    pop_args(4);
    return true;
}

def_prim(wifi_status, NoneToOneU32) {
    int32_t status = WiFi.status();
    pushInt32(status);
    return true;
}

def_prim(wifi_connected, NoneToOneU32) {
    pushInt32(WiFi.status() == WL_CONNECTED ? 1 : 0);
    return true;
}

def_prim(wifi_localip, twoToOneU32) {
    uint32_t buff = arg1.uint32;
    uint32_t size = arg0.uint32;
    IPAddress ip = WiFi.localIP();

    String ipString = String(ip[0]);
    for (byte octet = 1; octet < 4; ++octet) {
        ipString += '.' + String(ip[octet]);
    }

    // TODO handle too small buffer
    for (unsigned long i = 0; i < ipString.length(); i++) {
        m->memory.bytes[buff + i] = (uint32_t)ipString[i];
    }
    pop_args(2);
    pushInt32(buff);
    return true;
}

def_prim(http_get, fourToOneU32) {
    int32_t return_value = -11;

    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
        uint32_t addr = arg3.uint32;
        uint32_t length = arg2.uint32;
        uint32_t response = arg1.uint32;
        uint32_t size = arg0.uint32;

        String url = parse_utf8_string(m->memory.bytes, length, addr).c_str();
        Serial.print("GET ");
        Serial.println(url);

        Serial.flush();
        // Send HTTP GET request
        return_value = http_get_request(m, url, response, size);
    }

    pop_args(4);
    pushInt32(return_value);
    return true;
}

def_prim(http_post, tenToOneU32) {
    int32_t status_code = -11;

    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
        uint32_t url = arg9.uint32;
        uint32_t url_len = arg8.uint32;
        uint32_t body = arg7.uint32;
        uint32_t body_len = arg6.uint32;
        uint32_t content_type = arg5.uint32;
        uint32_t content_type_len = arg4.uint32;
        uint32_t authorization = arg3.uint32;
        uint32_t authorization_len = arg2.uint32;
        uint32_t response = arg1.uint32;
        uint32_t size = arg0.uint32;

        String url_parsed =
            parse_utf8_string(m->memory.bytes, url_len, url).c_str();
        String body_parsed =
            parse_utf8_string(m->memory.bytes, body_len, body).c_str();
        String content_type_parsed =
            parse_utf8_string(m->memory.bytes, content_type_len, content_type)
                .c_str();
        String authorization_parsed =
            parse_utf8_string(m->memory.bytes, authorization_len, authorization)
                .c_str();
        Serial.print("POST ");
        Serial.print(url_parsed);
        Serial.print("\n\t Content-type: '");
        Serial.print(content_type_parsed);
        Serial.print("'\n\t Authorization: '");
        Serial.print(authorization_parsed);
        Serial.print("'\n\t '");
        Serial.print(body_parsed);
        Serial.print("'\n");

        // Send HTTP POST request
        status_code =
            http_post_request(m, url_parsed, body_parsed, content_type_parsed,
                              authorization_parsed, response, size);
    }

    pop_args(10);
    pushInt32(status_code);
    Serial.flush();
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

def_prim(init_pixels, NoneToNoneU32) {
    pixels.begin();
    return true;
}

def_prim(set_pixel_color, fourToOneU32) {
    uint8_t blue = arg0.uint32;
    uint8_t green = arg1.uint32;
    uint8_t red = arg2.uint32;
    uint8_t index = arg3.uint32;

    pixels.setPixelColor(index, pixels.Color(red, green, blue));
    pop_args(4);
    return true;
}

def_prim(show_pixels, NoneToNoneU32) {
    pixels.show();
    return true;
}

def_prim(clear_pixels, NoneToNoneU32) {
    pixels.clear();
    return true;
}

// INTERRUPTS

class Interrupt {
   public:
    void setup(uint8_t pin, void (*ISR_callback)(void), uint8_t mode) {
        this->pin = pin;
        this->mode = mode;
        Serial.print("Attaching to ");
        Serial.print(pin);
        Serial.println("");
        attachInterrupt(digitalPinToInterrupt(pin), ISR_callback, mode);
    }

    void handleInterrupt();
    uint8_t pin;

   private:
    uint8_t mode;
    void (*ISR_callback)();
};

void Interrupt::handleInterrupt() {
    String topic = "interrupt";
    topic += String(pin);
    auto *empty = reinterpret_cast<const unsigned char *>("");
    CallbackHandler::push_event(topic.c_str(), empty, 0);
}

std::vector<Interrupt *> handlers;

def_prim(subscribe_interrupt, threeToNoneU32) {
    uint8_t pin = arg2.uint32;   // GPIOPin
    uint8_t fidx = arg1.uint32;  // Callback function
    uint8_t mode = arg0.uint32;

    Interrupt *handler = new Interrupt();
    handlers.push_back(handler);
    handler->setup(
        pin, [] { handlers.back()->handleInterrupt(); }, mode);

    String callback_id = "interrupt";
    callback_id += String(pin);
    Callback c = Callback(m, callback_id.c_str(), fidx);
    CallbackHandler::add_callback(c);

    pop_args(3);
    return true;
}

def_prim(unsubscribe_interrupt, oneToNoneU32) {
    uint8_t pin = arg0.uint32;

    auto it = std::remove_if(
        handlers.begin(), handlers.end(),
        [pin](Interrupt *handler) { return handler->pin == pin; });

    if (it != handlers.end()) {
        handlers.erase(it, handlers.end());
        detachInterrupt(digitalPinToInterrupt(pin));
    }

    pop_args(1);
    return true;
}

// MQTT MODULE

#include <PubSubClient.h>

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

def_prim(mqtt_init, threeToNoneU32) {
    WiFi.mode(WIFI_STA);
    uint32_t server_param = arg2.uint32;
    uint32_t length = arg1.uint32;
    uint32_t port = arg0.uint32;

    const char *server =
        parse_utf8_string(m->memory.bytes, length, server_param).c_str();
    mqttClient.setServer(server, port);
    mqttClient.setCallback([](const char *topic, const unsigned char *payload,
                              unsigned int length) {
        CallbackHandler::push_event(topic, payload, length);
    });

#if DEBUG
    Serial.print("Set MQTT server to [");
    Serial.print(server);
    Serial.print(":");
    Serial.print(port);
    Serial.println("]");
    Serial.flush();
#endif

    pop_args(3);
    return true;
}

def_prim(mqtt_connect, twoToOneU32) {
    uint32_t client_id_param = arg1.uint32;
    uint32_t length = arg0.uint32;

    String client_id =
        parse_utf8_string(m->memory.bytes, length, client_id_param).c_str();
#if DEBUG
    Serial.print("Connecting to MQTT server as ");
    Serial.print(client_id);
#endif
    bool ret = mqttClient.connect(client_id.c_str());

#if DEBUG
    Serial.print(": ");
    Serial.print(mqttClient.state());
    Serial.print(", ");
    Serial.println(ret);
    Serial.flush();
#endif

    pop_args(2);
    pushInt32((int)ret);
    return true;
}

def_prim(mqtt_connected, NoneToOneU32) {
    pushInt32((int)mqttClient.connected());
    return true;
}

def_prim(mqtt_state, NoneToOneU32) {
    pushInt32(mqttClient.state());
    return true;
}

def_prim(mqtt_publish, fourToOneU32) {
    uint32_t topic_param = arg3.uint32;
    uint32_t topic_length = arg2.uint32;
    uint32_t payload_param = arg1.uint32;
    uint32_t payload_length = arg0.uint32;

    String topic =
        parse_utf8_string(m->memory.bytes, topic_length, topic_param).c_str();
#if DEBUG
    Serial.println("MQTT Publish");
    Serial.print("(");
    Serial.print(topic_param);
    Serial.print(", ");
    Serial.print(topic_length);
    Serial.print(") ");
    Serial.print(topic);
    Serial.println("");
#endif
    String payload =
        parse_utf8_string(m->memory.bytes, payload_length, payload_param)
            .c_str();
#if DEBUG
    Serial.print("(");
    Serial.print(payload_param);
    Serial.print(", ");
    Serial.print(payload_length);
    Serial.print(") ");
    Serial.print(payload);
    Serial.println("");
#endif

    bool ret = mqttClient.publish(topic.c_str(), payload.c_str());

#if DEBUG
    Serial.print("Status code: ");
    Serial.println(ret);
    Serial.flush();
#endif

    pop_args(4);
    pushInt32((int)ret);
    return true;
}

def_prim(mqtt_subscribe, threeToOneU32) {
    uint32_t topic_param = arg2.uint32;
    uint32_t topic_length = arg1.uint32;
    uint32_t fidx = arg0.uint32;

    const char *topic =
        parse_utf8_string(m->memory.bytes, topic_length, topic_param).c_str();

    Callback c = Callback(m, topic, fidx);
    CallbackHandler::add_callback(c);

    bool ret = mqttClient.subscribe(topic);

#if DEBUG
    Serial.print("Subscribed to ");
    Serial.println(topic);
    Serial.flush();
#endif

    pop_args(2);
    pushInt32((int)ret);
    return true;
}

def_prim(mqtt_unsubscribe, threeToOneU32) {
    uint32_t topic_param = arg2.uint32;
    uint32_t topic_length = arg1.uint32;
    uint32_t fidx = arg0.uint32;

    const char *topic =
        parse_utf8_string(m->memory.bytes, topic_length, topic_param).c_str();

    Callback c = Callback(m, topic, fidx);
    CallbackHandler::remove_callback(c);

    bool ret = 1;
    if (CallbackHandler::callback_count(topic) == 0) {
        ret = mqttClient.unsubscribe(topic);
    }

#if DEBUG
    Serial.print("Unsubscribed to ");
    Serial.println(topic);
    Serial.flush();
#endif

    pop_args(2);
    pushInt32((int)ret);
    return true;
}

def_prim(mqtt_loop, NoneToOneU32) {
    pushInt32((int)mqttClient.loop());
    return true;
}

#else

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
    CallbackHandler::push_event(topic, payload, 11);
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
    printf("%s\n", text.c_str());
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
    uint32_t size = arg0.uint32;
    std::string ip = "192.168.0.181";

    for (unsigned long i = 0; i < ip.length(); i++) {
        m->memory.bytes[buff + i] = (uint32_t)ip[i];
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
        m->memory.bytes[response + i] = (uint32_t)answer[i];
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
    uint32_t size = arg0.uint32;

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
    debug("EMU: chip_digital_write(%u,%u) \n", arg1.uint32, arg0.uint32);
    pop_args(2);
    return true;
}

def_prim(chip_digital_read, oneToOneU32) {
    uint8_t pin = arg0.uint32;
    pop_args(1);
    pushUInt32(1);  // HIGH
    return true;
}

def_prim(chip_analog_read, oneToOneI32) {
    uint8_t pin = arg0.uint32;
    pop_args(1);
    pushInt32(sin(sensor_emu) * 100);
    sensor_emu += .25;
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

#endif

//------------------------------------------------------
// Util functions
//------------------------------------------------------
#ifdef ARDUINO

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
    // install_primitive(rand);
#ifdef ARDUINO
    dbg_info("INSTALLING ARDUINO\n");
    install_primitive(abort);
    install_primitive(millis);
    install_primitive(micros);

    install_primitive(print_int);
    install_primitive(print_string);

    install_primitive(wifi_connect);
    install_primitive(wifi_connected);
    install_primitive(wifi_status);
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
    install_primitive(unsubscribe_interrupt);

    install_primitive(mqtt_init);
    install_primitive(mqtt_connect);
    install_primitive(mqtt_connected);
    install_primitive(mqtt_state);
    install_primitive(mqtt_publish);
    install_primitive(mqtt_subscribe);
    install_primitive(mqtt_unsubscribe);
    install_primitive(mqtt_loop);

    install_primitive(init_pixels);
    install_primitive(set_pixel_color);
    install_primitive(clear_pixels);
    install_primitive(show_pixels);

#else
    dbg_info("INSTALLING FAKE ARDUINO\n");
    install_primitive(abort);
    install_primitive(micros);
    install_primitive(test);
    install_primitive(print_int);
    install_primitive(print_string);
    install_primitive(wifi_connect);
    install_primitive(wifi_connected);
    install_primitive(wifi_status);
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

    install_primitive(init_pixels);
    install_primitive(set_pixel_color);
    install_primitive(clear_pixels);
    install_primitive(show_pixels);

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
