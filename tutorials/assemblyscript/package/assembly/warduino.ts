// Time primitives

@external("env", "millis")              export declare function _millis(): u32;
@external("env", "chip_delay")          export declare function _delay(ms: u32): void;

// Digital I/O primitives

@external("env", "chip_pin_mode")       export declare function _pin_mode(pin: u32, mode: u32): void;
@external("env", "chip_digital_write")  export declare function _digital_write(pin: u32, value: u32): void;
@external("env", "chip_digital_read")   export declare function _digital_read(pin: u32): u32;

@external("env", "chip_analog_read")    export declare function _analog_read(pin: u32): i32;
@external("env", "chip_analog_write")   export declare function _analog_write(pin: u32, signal: u32): i32;

// LED Control primitives

@external("env", "chip_ledc_setup")        export declare function _analog_setup(channel: u32, frequency: u32, timer: u32): void;
@external("env", "chip_ledc_attach_pin")   export declare function _analog_attach(pin: u32, channel: u32): void;
@external("env", "chip_ledc_set_duty")     export declare function _analog_duty(channel: u32, value: i32, max: u32): void;

// Interrupts primitives

@external("env", "subscribe_interrupt") export declare function _subscribe_interrupt(pin: u32, fn: (topic: string, payload: string) => void, mode: u32): void;

// Serial
@external("env", "print_string")        export declare function _print(text: ArrayBuffer, length: u32): void;

// Wi-Fi
@external("env", "wifi_connect")        export declare function _wifi_connect(ssid: ArrayBuffer, length: i32, password: ArrayBuffer, size: i32): void;
@external("env", "wifi_status")         export declare function _wifi_status(): i32;
@external("env", "wifi_localip")        export declare function _wifi_localip(buff: ArrayBuffer, buffer_size: u32): i32;

// HTTP
@external("env", "http_get")            export declare function _http_get(url: ArrayBuffer, url_len: u32, buffer: ArrayBuffer, buffer_size: u32): i32;
@external("env", "http_post")           export declare function _http_post(url: ArrayBuffer, url_len: u32,
                                                                    body: ArrayBuffer, body_len: u32,
                                                                    content_type: ArrayBuffer, content_type_len: u32,
                                                                    authorization: ArrayBuffer, authorization_len: u32,
                                                                    buffer: ArrayBuffer, buffer_size: u32): i32;

// MQTT
@external("env", "mqtt_init")        export declare function _mqtt_init(server: ArrayBuffer, length: i32, port: u32): void;
@external("env", "mqtt_connect")     export declare function _mqtt_connect(client_id: ArrayBuffer, length: i32): i32;
@external("env", "mqtt_connected")   export declare function _mqtt_connected(): i32;
@external("env", "mqtt_state")       export declare function _mqtt_state(): i32;

@external("env", "mqtt_publish")     export declare function _mqtt_publish(topic: ArrayBuffer, topic_length: i32, payload: ArrayBuffer, payload_length: i32): i32;
@external("env", "mqtt_subscribe")   export declare function _mqtt_subscribe(topic: string, fn: (topic: string, payload: string) => void): i32;
@external("env", "mqtt_unsubscribe") export declare function _mqtt_unsubscribe(topic: string, fn: (topic: string, payload: string) => void): i32;
@external("env", "mqtt_loop")        export declare function _mqtt_loop(): i32;

//Serial
@external("env", "serial_begin")      export declare function _serial_begin(baud_rate: i32): void;

@external("env", "set_initialised")      export declare function _set_initialised(): void;
@external("env", "check_initialised")    export declare function _check_initialised(): boolean;


// RadioLib
@external("env", "radio_begin")          export declare function _radio_begin(frequency: i32, bandwidth: i32, spreadingFactor: i32, codingRate: i32, syncWord: i32, outputPower: i32): u32;
@external("env", "radio_transmit")       export declare function _radio_transmit(data: ArrayBuffer, length: u32): i32;
@external("env", "radio_startReceive")   export declare function _radio_startReceive(): i32;
@external("env", "radio_receive")        export declare function _radio_receive(buffer: ArrayBuffer, buffer_size: u32): i32;

// M5Cardputer
@external("env", "m5_cardputer_begin")                  export declare function _m5_cardputer_begin(): void;
@external("env", "m5_cardputer_display_setRotation")    export declare function _m5_cardputer_display_setRotation(rotation: i32): void;
@external("env", "m5_cardputer_display_fillScreen")     export declare function _m5_cardputer_display_fillScreen(color: i32): void;
@external("env", "m5_cardputer_display_setTextColor")   export declare function _m5_cardputer_display_setTextColor(color: i32): void;
@external("env", "m5_cardputer_display_setTextSize")    export declare function _m5_cardputer_display_setTextSize(size: i32): void;
@external("env", "m5_cardputer_display_setCursor")      export declare function _m5_cardputer_display_setCursor(x: i32, y: i32): void;
@external("env", "m5_cardputer_display_println")        export declare function _m5_cardputer_display_println(text: ArrayBuffer, length: u32): i32;
@external("env", "m5_cardputer_display_print")          export declare function _m5_cardputer_display_print(text: ArrayBuffer, length: u32): i32;

// Neopixel
@external("env", "init_pixels")      export declare function _init_pixels(): void;
@external("env", "clear_pixels")     export declare function _clear_pixels(): void;
@external("env", "set_pixel_color")  export declare function _set_pixel_color(ledIndex: i32, r: i32, g: i32, b:i32): void;
@external("env", "show_pixels")      export declare function _show_pixels(): void;

