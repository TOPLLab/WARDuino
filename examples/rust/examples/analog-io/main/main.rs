use warduino::{analog_read, analog_write, delay, print};

static IN: u32 = 13;
static OUT: u32 = 9;

fn map(value: u32, x1: u32, y1: u32, x2: u32, y2: u32) -> u32 {
    (value - x1) * (y2 - x2) / (y1 - x1) + x2
}

#[no_mangle]
pub fn main() {
    let value: u32 = analog_read(IN);
    let output: u32 = map(value, 0, 1023, 0, 255);

    analog_write(OUT, output);

    print(&format!("sensor = {:#?}\t output = {:#?}", value, output).to_string());
    delay(1);
}
