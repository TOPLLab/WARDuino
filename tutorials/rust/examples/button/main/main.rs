use warduino::{delay,
               digital_read,
               digital_write,
               InterruptMode,
               pin_mode,
               PinMode,
               PinVoltage,
               sub_interrupt};

static BUTTON: u32 = 25;
static LED: u32 = 26;

fn callback(_topic: &str, _payload: &str, _length: u32) {
    let voltage = digital_read(LED);
    match voltage {
        PinVoltage::HIGH => digital_write(LED, PinVoltage::LOW),
        PinVoltage::LOW => digital_write(LED, PinVoltage::HIGH)
    }
}

#[no_mangle]
pub fn main() {
    pin_mode(BUTTON, PinMode::INPUT);
    pin_mode(LED, PinMode::OUTPUT);

    sub_interrupt(BUTTON, InterruptMode::FALLING, callback);

    loop {
        delay(1000);
    }
}
