#![allow(dead_code)]

use std::mem;

#[link(wasm_import_module = "env")]
//#[link(wasm_import_module = "arduino")]
extern {
    #[link_name = "test"]                   pub fn test(f: fn(&str, &str, u32));

    #[link_name = "millis"]                 fn _millis() -> u32;
    #[link_name = "micros"]                 fn _micros() -> u64;
    #[link_name = "chip_delay"]             fn _delay(ms: u32);
    #[link_name = "getPinLED"]              fn _getPinLED() -> u32;
    #[link_name = "chip_pin_mode"]          fn _pinMode(pin: u32, mode: u32);
    #[link_name = "chip_digital_write"]     fn _digitalWrite(pin: u32, value: u32);
    #[link_name = "chip_digital_read"]      fn _digitalRead(pin: u32) -> u32;
    #[link_name = "chip_analog_read"]       fn _analogRead(pin: u32) -> i32;
//}

// Benchmark
    #[link_name = "bench_start"]            pub fn _bench_start();
    #[link_name = "bench_finish"]           pub fn _bench_finish() -> u64;


//#[link(wasm_import_module = "serial")]
//extern {
    #[link_name = "print_string"]       fn _print_buffer(text: *const u8, length: usize);
    #[link_name = "print_int"]          fn _print_int(integer: i32);
//}

//#[link(wasm_import_module = "wifi")]
//extern {
    #[link_name = "wifi_connect"]      fn _connect(ssid: &str, password: &str);
    #[link_name = "wifi_status"]       fn _status() -> i32;
    #[link_name = "wifi_localip"]      fn _localip(buffer: *const u8, buffer_length: usize) -> i32;
//}

//#[link(wasm_import_module = "http")]
//extern {
    #[link_name = "http_get"]           fn _get(url: *const u8, url_len: usize, buffer: *const u8, buffer_size: usize) -> i32;
    #[link_name = "http_post"]          fn _post(url: *const u8, url_len: usize,
                                                 body: *const u8, body_len: usize,
                                                 content_type: *const u8, content_type_len: usize,
                                                 authorization: *const u8, authorization_len: usize,
                                                 buffer: *const u8, buffer_size: usize)
                                           -> i32;
    

//#[link(wasm_import_module = "interrupt")]
//extern {
    #[link_name = "subscribe_interrupt"]  fn _sub_interrupt(pin: u32, f: fn(&str, &str, u32), mode: u32);

//#[link(wasm_import_module = "mqtt")]
//extern {
    #[link_name = "mqtt_init"]          fn _mqtt_init(server: *const u8, server_length: usize, port: u32);
    #[link_name = "mqtt_connect"]       fn _mqtt_connect(client_id: *const u8, client_id_length: usize) -> i32;
    #[link_name = "mqtt_connected"]     fn _mqtt_connected() -> i32;
    #[link_name = "mqtt_state"]         fn _mqtt_state() -> i32;
    #[link_name = "mqtt_publish"]       fn _mqtt_publish(topic: *const u8, topic_length: usize, payload: *const u8, payload_length: usize) -> i32;
    #[link_name = "mqtt_subscribe"]     fn _mqtt_subscribe(topic: *const u8, topic_length: usize, f: fn(&str, &str, u32)) -> i32;
    #[link_name = "mqtt_unsubscribe"]   fn _mqtt_unsubscribe(topic: *const u8, topic_length: usize, f: fn(&str, &str, u32)) -> i32;
    #[link_name = "mqtt_loop"]          fn _mqtt_loop() -> i32;
}

#[repr(C)]
pub struct Headers {
    pub content_type: &'static str,
    pub authorization: &'static str,  // TODO make optional
}

#[repr(C)]
pub struct PostOptions {
    pub uri: &'static str,
    pub body: String,
    pub headers: Headers,
}

fn size_of_post_options(options: &PostOptions) -> usize {
    options.uri.len() + options.body.len() + options.headers.content_type.len()
}

pub static LOW     : u32 = 0x0;
pub static HIGH    : u32 = 0x1;

pub static CHANGE  : u32 = 1;
pub static FALLING : u32 = 2;
pub static RISING  : u32 = 3;

pub static INPUT   : u32 = 0x0;
pub static OUTPUT  : u32 = 0x2;

pub fn millis       () -> u32               { unsafe { _millis() } }
pub fn micros       () -> u64               { unsafe { _micros() } }
pub fn delay        (ms: u32)               { unsafe { _delay(ms); } }
pub fn get_pin_led  () -> u32               { unsafe { _getPinLED() } }
pub fn pin_mode     (pin: u32, mode: u32)   { unsafe { _pinMode(pin, mode) } }
pub fn digital_write(pin: u32, value: u32)  { unsafe { _digitalWrite(pin, value) } }
pub fn digital_read (pin: u32) -> u32       { unsafe { _digitalRead(pin) } }
pub fn analog_read  (pin: u32) -> i32       { unsafe { _analogRead(pin) } }

pub fn bench_start  ()        { unsafe { _bench_start() } }
pub fn bench_finish () -> u64 { unsafe { _bench_finish() } }

pub fn wifi_connect (ssid: &str, password: &str)            { unsafe { _connect(ssid, password) } }
pub fn wifi_status  () -> i32                               { unsafe { _status() } }
pub fn wifi_localip () -> String                            { unsafe { let buffer: [u8; 100] = [0; 100];
                                                                       _localip(buffer.as_ptr(), mem::size_of_val(&buffer) / mem::size_of::<u8>());
                                                                       std::str::from_utf8(&buffer).unwrap().to_owned()
                                                            } }

pub fn get  (url: &str, buffer: &[u8]) -> i32               { unsafe { _get(url.as_ptr(), url.len(), buffer.as_ptr(), mem::size_of_val(buffer) / mem::size_of::<u8>()) } }
pub fn post (options: &PostOptions, buffer: &[u8]) -> i32   { unsafe {
        _post(options.uri.as_ptr(), options.uri.len(),
              options.body.as_ptr(), options.body.len(),
              options.headers.content_type.as_ptr(), options.headers.content_type.len(),
              options.headers.authorization.as_ptr(), options.headers.authorization.len(),
              buffer.as_ptr(), mem::size_of_val(buffer) / mem::size_of::<u8>())
    }
}

pub fn print        (text: &[u8])   { unsafe { _print_buffer(text.as_ptr(), text.len()) } }
pub fn print_int    (integer: i32)  { unsafe { _print_int(integer) } }

pub fn sub_interrupt    (pin: u32, f: fn(&str, &str, u32), mode: u32)    { unsafe { _sub_interrupt(pin, f, mode) } }

pub fn mqtt_init        (server: &str, port: u32)                       { unsafe { _mqtt_init(server.as_ptr(), server.len(), port) } }
pub fn mqtt_connect     (client_id: &str) -> bool                       { unsafe { _mqtt_connect(client_id.as_ptr(), client_id.len()) != 0 } }
pub fn mqtt_connected   () -> bool                                      { unsafe { _mqtt_connected() > 0 } }
pub fn mqtt_state       () -> i32                                       { unsafe { _mqtt_state() } }
pub fn mqtt_publish     (topic: &str, payload: &str) -> i32             { unsafe { _mqtt_publish(topic.as_ptr(), topic.len(), payload.as_ptr(), payload.len()) } }
pub fn mqtt_subscribe   (topic: &str, f: fn(&str, &str, u32)) -> i32    { unsafe { _mqtt_subscribe(topic.as_ptr(), topic.len(), f) } }
pub fn mqtt_unsubscribe (topic: &str, f: fn(&str, &str, u32)) -> i32    { unsafe { _mqtt_unsubscribe(topic.as_ptr(), topic.len(), f) } }
pub fn mqtt_loop        () -> i32                                       { unsafe { _mqtt_loop() } }

