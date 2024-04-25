use warduino::{pin_mode, PinMode};

mod config;

#[no_mangle]
pub fn main() {
    let led: u32 = config::LED;
    pin_mode(led, PinMode::OUTPUT);
}
