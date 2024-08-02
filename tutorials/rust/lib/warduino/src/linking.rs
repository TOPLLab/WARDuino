#[link(wasm_import_module = "env")]
extern {
    // Time
    #[link_name = "millis"]
    pub fn _millis() -> u32;
    #[link_name = "chip_delay"]
    pub fn _delay(ms: u32);

    // Digital I/O
    #[link_name = "chip_pin_mode"]
    pub fn _pinMode(pin: u32, mode: u32);
    #[link_name = "chip_digital_write"]
    pub fn _digitalWrite(pin: u32, value: u32);
    #[link_name = "chip_digital_read"]
    pub fn _digitalRead(pin: u32) -> u32;

    // Analog I/O
    #[link_name = "chip_analog_read"]
    pub fn _analogRead(pin: u32) -> u32;
    #[link_name = "chip_analog_write"]
    pub fn _analogWrite(pin: u32, signal: u32) -> u32;

    // Serial
    #[link_name = "print_string"]
    pub fn _print_buffer(text: *const u8, length: usize);
    #[link_name = "print_int"]
    pub fn _print_int(integer: i32);

    // Wi-Fi
    #[link_name = "wifi_connect"]
    pub fn _connect(ssid: &str, password: &str);
    #[link_name = "wifi_status"]
    pub fn _status() -> i32;
    #[link_name = "wifi_localip"]
    pub fn _localip(buffer: *const u8, buffer_length: usize) -> i32;

    // HTTP
    #[link_name = "http_get"]
    pub fn _get(url: *const u8, url_len: usize, buffer: *const u8, buffer_size: usize) -> i32;
    #[link_name = "http_post"]
    pub fn _post(url: *const u8, url_len: usize,
             body: *const u8, body_len: usize,
             content_type: *const u8, content_type_len: usize,
             authorization: *const u8, authorization_len: usize,
             buffer: *const u8, buffer_size: usize)
             -> i32;

    // Interrupts
    #[link_name = "subscribe_interrupt"]
    pub fn _sub_interrupt(pin: u32, f: fn(&str, &str, u32), mode: u32);
    #[link_name = "unsubscribe_interrupt"]
    pub fn _unsub_interrupt(pin: u32);

    // MQTT
    #[link_name = "mqtt_init"]
    pub fn _mqtt_init(server: *const u8, server_length: usize, port: u32);
    #[link_name = "mqtt_connect"]
    pub fn _mqtt_connect(client_id: *const u8, client_id_length: usize) -> i32;
    #[link_name = "mqtt_connected"]
    pub fn _mqtt_connected() -> i32;
    #[link_name = "mqtt_state"]
    pub fn _mqtt_state() -> i32;
    #[link_name = "mqtt_publish"]
    pub fn _mqtt_publish(topic: *const u8, topic_length: usize, payload: *const u8, payload_length: usize) -> i32;
    #[link_name = "mqtt_subscribe"]
    pub fn _mqtt_subscribe(topic: *const u8, topic_length: usize, f: fn(&str, &str, u32)) -> i32;
    #[link_name = "mqtt_unsubscribe"]
    pub fn _mqtt_unsubscribe(topic: *const u8, topic_length: usize, f: fn(&str, &str, u32)) -> i32;
    #[link_name = "mqtt_loop"]
    pub fn _mqtt_loop() -> i32;
}
