@external("env", "millis")              export declare function millis(): u32;
@external("env", "chip_delay")          export declare function delay(ms: u32): void;
@external("env", "chip_pin_mode")       export declare function pin_mode(pin: u32, mode: u32): void;
@external("env", "chip_digital_write")  export declare function digital_write(pin: u32, value: u32): void;
@external("env", "chip_digital_read")   export declare function digital_read(pin: u32): u32;
@external("env", "chip_analog_read")    export declare function analog_read(pin: u32): i32;

@external("env", "subscribe_interrupt") declare function subscribe_interrupt(pin: u32, fn: () => void, mode: u32): void;

@external("env", "print_string")        declare function _print(text: ArrayBuffer, length: u32): void;

@external("env", "wifi_connect")        declare function _wifi_connect(ssid: ArrayBuffer, length: i32, password: ArrayBuffer, size: i32): void;
@external("env", "wifi_status")         export declare function wifi_status(): i32;
@external("env", "wifi_localip")        declare function _wifi_localip(buff: ArrayBuffer, buffer_size: u32): i32;

@external("env", "http_get")            declare function _http_get(url: ArrayBuffer, url_len: u32, buffer: ArrayBuffer, buffer_size: u32): i32;
@external("env", "http_post")           declare function _http_post(url: ArrayBuffer, url_len: u32,
                                                                    body: ArrayBuffer, body_len: u32,
                                                                    content_type: ArrayBuffer, content_type_len: u32,
                                                                    authorization: ArrayBuffer, authorization_len: u32,
                                                                    buffer: ArrayBuffer, buffer_size: u32): i32;

@external("env", "mqtt_init")        declare function  _mqtt_init(server: ArrayBuffer, length: i32, port: u32): void;
@external("env", "mqtt_connect")     declare function  _mqtt_connect(client_id: ArrayBuffer, length: i32): i32;
@external("env", "mqtt_connected")   declare function  _mqtt_connected(): i32;
@external("env", "mqtt_state")       export declare function  mqtt_state(): i32;
@external("env", "mqtt_publish")     declare function  _mqtt_publish(topic: ArrayBuffer, length: i32, payload: ArrayBuffer, length: i32): i32;
@external("env", "mqtt_subscribe")   declare function  _mqtt_subscribe(topic: ArrayBuffer, length: i32, fn: (topic: ArrayBuffer, topic_length: i32, payload: ArrayBuffer, payload_length: i32) => void): i32;
@external("env", "mqtt_unsubscribe") declare function  _mqtt_unsubscribe(topic: ArrayBuffer, length: i32, fn: (topic: ArrayBuffer, topic_length: i32, payload: Arraybuffer, payload_length: i32) => void): i32;
@external("env", "mqtt_loop")        export declare function  mqtt_loop(): i32;

export const LOW: u32 = 0;
export const HIGH: u32 = 1;

export const CHANGED: u32 = 1;
export const FALLING: u32 = 2;
export const RISING: u32 = 3;

export const INPUT: u32 = 0x0;
export const OUTPUT: u32 = 0x2;

export const WL_CONNECTED: u32 = 3;

export function sleep(s: u32): void {
    delay(s * 1000);
}

export function interrupt_on(pin: u32, mode: u32, fn: () => void): void {
    subscribe_interrupt(pin, fn, mode);
}

export function print(text: string): void {
    _print(String.UTF8.encode(text, true), String.UTF8.byteLength(text, true));
}

export function wifi_connect(ssid: string, password: string): void {
    _wifi_connect(String.UTF8.encode(ssid, true), String.UTF8.byteLength(ssid, true),
                  String.UTF8.encode(password, true), String.UTF8.byteLength(password, true));
}

export function wifi_localip(): string {
    let localip = new ArrayBuffer(20);
    _wifi_localip(localip, localip.byteLength);
    return String.UTF8.decode(localip, true);
}

export function http_get(url: string, buffer: ArrayBuffer): i32 {
    return _http_get(String.UTF8.encode(url, true), String.UTF8.byteLength(url, true), buffer, buffer.byteLength);
}

export function http_post(url: string, body: string, content_type: string, authorization: string, response: ArrayBuffer): i32 {
    return _http_post(String.UTF8.encode(url, true), String.UTF8.byteLength(url, true),
                      String.UTF8.encode(body, true), String.UTF8.byteLength(body, true),
                      String.UTF8.encode(content_type, true), String.UTF8.byteLength(content_type, true),
                      String.UTF8.encode(authorization, true), String.UTF8.byteLength(authorization, true),
                      response, response.byteLength);
}

export function mqtt_init(server: string, port: u32): void {
    _mqtt_init(String.UTF8.encode(server, true), String.UTF8.byteLength(server, true), port);
}

export function mqtt_connect(client_id: string): i32 {
    return _mqtt_connect(String.UTF8.encode(client_id, true), String.UTF8.byteLength(client_id, true));
}

export function mqtt_connected(): bool {
    return _mqtt_connected() == 1;
}

export function mqtt_publish(topic: string, payload: string): i32 {
    return _mqtt_publish(String.UTF8.encode(topic, true), String.UTF8.byteLength(topic, true), String.UTF8.encode(payload, true), String.UTF8.byteLength(payload, true));
}

export function mqtt_subscribe(topic: string, callback: (topic: string, payload: string) => void): i32 {
    return _mqtt_subscribe(String.UTF8.encode(topic, true),
                           String.UTF8.byteLength(topic, true),
                           (topic: ArrayBuffer, topic_length: i32, payload: ArrayBuffer, payload_length: i32) => { callback(String.UTF8.decode(topic, true), String.UTF8.decode(payload, true)); } );
}

export function mqtt_unsubscribe(topic: string, callback: (topic: string, payload: string) => void): i32 {
    return _mqtt_unsubscribe(String.UTF8.encode(topic, true),
                           String.UTF8.byteLength(topic, true),
                           (topic: ArrayBuffer, topic_length: i32, payload: ArrayBuffer, payload_length: i32) => { callback(String.UTF8.decode(topic, true), String.UTF8.decode(payload, true)); } );
}
