use warduino::{analog_read, delay, print};

#[no_mangle]
pub fn main() {
    let value: u32 = analog_read(13);
    print(&value.to_string());
    delay(1);
}
