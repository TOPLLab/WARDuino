//! warduino - WARDuino primitives library
//!
//! <br>
//!
//! WARDuino is a dynamic WebAssembly runtime for embedded devices, that
//! provides primitives to access hardware and IoT specific functionality.
//!
//! The `warduino` crate allows using these primitives from Rust.
//!
//! <br>
//!
//! # Usage
//!
//! ```toml
//! [dependencies]
//! warduino = "0.1.0"
//! ```
//!
//! ```rust
//! use warduino::{PinMode, PinVoltage, pin_mode, digital_write, delay};
//!
//! pub fn main() {
//!     let led = 2;
//!     pin_mode(led, PinMode::OUTPUT);
//!
//!     let pause = 1000;
//!     loop {
//!         digital_write(led, PinVoltage::HIGH);
//!         delay(pause);
//!         digital_write(led, PinVoltage::LOW);
//!         delay(pause);
//!     }
//! }
//! ```

#![crate_name = "warduino"]

extern crate num;
#[macro_use]
extern crate num_derive;

use linking::*;
use std::mem;

mod linking;

#[repr(C)]
pub struct Headers {
    pub content_type: &'static str,
    pub authorization: &'static str,
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

/// The voltage of a digital pin.
#[derive(FromPrimitive)]
pub enum PinVoltage {
    /// Low voltage on a digital I/O pin
    LOW = 0x0,
    /// High voltage on a digital I/O pin
    HIGH = 0x1,
}

/// The mode of a pin interrupt.
#[derive(FromPrimitive)]
pub enum InterruptMode {
    /// Changing edge on a digital I/O pin
    CHANGE = 1,
    /// Falling edge on a digital I/O pin
    FALLING = 2,
    /// Rising edge on a digital I/O pin
    RISING = 3,
}

/// The mode of a digital I/O pin.
#[derive(FromPrimitive)]
pub enum PinMode {
    /// Input mode for digital pins
    INPUT = 0x0,
    /// Output mode for digital pins
    OUTPUT = 0x2,
}

/// Returns the number of milliseconds passed since the current program started to run.
pub fn millis() -> u32 { unsafe { _millis() } }

/// Pauses the program for the amount of time (in milliseconds).
pub fn delay(ms: u32) { unsafe { _delay(ms); } }

/// Pauses the program for the amount of time (in seconds)
pub fn sleep(s: u32) { delay(s * 1000); }

/// Configures the [PinMode] of the specified pin.
pub fn pin_mode(pin: u32, mode: PinMode) { unsafe { _pinMode(pin, mode as u32) } }

/// Write the voltage to a specified digital pin, either [HIGH](PinVoltage) or [LOW](PinVoltage).
pub fn digital_write(pin: u32, value: PinVoltage) { unsafe { _digitalWrite(pin, value as u32) } }

/// Reads the value from a specified digital pin, either [HIGH](PinVoltage) or [LOW](PinVoltage).
pub fn digital_read(pin: u32) -> PinVoltage { unsafe { num::FromPrimitive::from_u32(_digitalRead(pin)).unwrap() } }

/// Reads the value from the specified analog pin.
pub fn analog_read(pin: u32) -> u32 { unsafe { _analogRead(pin) } }

/// Writes the value to the specified analog pin.
pub fn analog_write(pin: u32, signal: u32) -> u32 { unsafe { _analogWrite(pin, signal) } }

/// The status of the Wi-Fi connection
#[derive(FromPrimitive, PartialEq)]
pub enum WiFiStatus {
    /// No Wi-Fi hardware found
    NoShield = 255,
    /// Wi-Fi is in process of changing between statuses
    Idle = 0,
    /// Configured SSID cannot be reached
    SsidUnavailable = 1,
    ///
    ScanCompleted = 2,
    /// Successful connection is established
    Connected = 3,
    /// Failed to connect
    ConnectFailed = 4,
    /// No longer connected
    ConnectionLost = 5,
    /// Module is not configured in station mode
    Disconnected = 6,
}

/// Connect to Wi-Fi network with SSID and password
pub fn wifi_connect(ssid: &str, password: &str) { unsafe { _connect(ssid, password) } }

/// Returns the status of the Wi-Fi connection of the board
pub fn wifi_status() -> WiFiStatus { unsafe { num::FromPrimitive::from_i32(_status()).unwrap() } }

/// Returns whether the board si still connected to Wi-Fi
pub fn wifi_connected() -> bool { wifi_status() == WiFiStatus::Connected }

/// Returns the local IP address of the board
pub fn wifi_localip() -> String {
    unsafe {
        let buffer: [u8; 100] = [0; 100];
        _localip(buffer.as_ptr(), mem::size_of_val(&buffer) / mem::size_of::<u8>());
        std::str::from_utf8(&buffer).unwrap().to_owned()
    }
}

const BUFFER_SIZE: usize = 250;

/// Send an HTTP GET request.
pub fn get(url: &str) -> &str {
    const BUFFER: &[u8; BUFFER_SIZE] = &[0; BUFFER_SIZE];
    unsafe { 
        _get(url.as_ptr(), url.len(), BUFFER.as_ptr(), mem::size_of_val(BUFFER) / mem::size_of::<u8>());
        std::str::from_utf8_unchecked(BUFFER)
    }
}

/// Send an HTTP POST request.
pub fn post(options: &PostOptions) -> &str {
    const BUFFER: &[u8; BUFFER_SIZE] = &[0; BUFFER_SIZE];
    unsafe {
        _post(options.uri.as_ptr(), options.uri.len(),
              options.body.as_ptr(), options.body.len(),
              options.headers.content_type.as_ptr(), options.headers.content_type.len(),
              options.headers.authorization.as_ptr(), options.headers.authorization.len(),
              BUFFER.as_ptr(), mem::size_of_val(BUFFER) / mem::size_of::<u8>());
        std::str::from_utf8_unchecked(BUFFER)
    }

}

/// Print a string to the serial port.
pub fn print(text: &str) { unsafe { _print_buffer(text.as_ptr(), text.len()) } }

/// Print an integer to the serial port.
pub fn print_int(integer: i32) { unsafe { _print_int(integer) } }

/// subscribe a callback function to an interrupt on the given pin
pub fn sub_interrupt(pin: u32, mode: InterruptMode, f: fn(&str, &str, u32)) { unsafe { _sub_interrupt(pin, f, mode as u32) } }

/// Unsubscribe all callback functions for a given pin
pub fn unsub_interrupt(pin: u32) { unsafe { _unsub_interrupt(pin) } }

/// The status of the MQTT connection
#[derive(FromPrimitive, PartialEq)]
pub enum MQTTStatus {
    /// The server didn't respond within the keepalive time
    ConnectionTimeout = -4,
    /// The network connection was broken
    ConnectionLost = -3,
    /// The network connection failed
    ConnectFailed = -2,
    /// The client is disconnected cleanly
    Disconnected = -1,
    /// The client is connected
    Connected = 0,
    /// the server doesn't support the requested version of MQTT
    ConnectBadProtocol = 1,
    /// The server rejected the client identifier
    ConnectBadClientId = 2,
    /// The server was unable to accept the connection
    ConnectUnavailable = 3,
    /// The username/password were rejected
    ConnectBadCredentials = 4,
    /// The client was not authorized to connect
    ConnectUnauthorized = 5,
}

/// Configure a MQTT broker
pub fn mqtt_init(server: &str, port: u32) { unsafe { _mqtt_init(server.as_ptr(), server.len(), port) } }

/// Connect to the Configured MQTT broker with client_id
pub fn mqtt_connect(client_id: &str) -> bool { unsafe { _mqtt_connect(client_id.as_ptr(), client_id.len()) != 0 } }

/// Returns whether the board is still connected to the MQTT broker
pub fn mqtt_connected() -> bool { mqtt_state() == MQTTStatus::Connected }

/// Returns the status of the connection to the MQTT broker
pub fn mqtt_state() -> MQTTStatus { unsafe { num::FromPrimitive::from_i32(_mqtt_state()).unwrap() } }

/// Publish a message on an MQTT topic
pub fn mqtt_publish(topic: &str, payload: &str) -> i32 { unsafe { _mqtt_publish(topic.as_ptr(), topic.len(), payload.as_ptr(), payload.len()) } }

/// Subscribe a callback function to an MQTT topic
pub fn mqtt_subscribe(topic: &str, f: fn(&str, &str, u32)) -> i32 { unsafe { _mqtt_subscribe(topic.as_ptr(), topic.len(), f) } }

/// Unsubscribe a callback function from an MQTT topic
pub fn mqtt_unsubscribe(topic: &str, f: fn(&str, &str, u32)) -> i32 { unsafe { _mqtt_unsubscribe(topic.as_ptr(), topic.len(), f) } }

/// Check for messages from the MQTT broker
pub fn mqtt_poll() -> i32 { unsafe { _mqtt_loop() } }

