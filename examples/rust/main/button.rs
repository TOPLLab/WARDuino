use warduino::*;

static BUTTON : u32 = 25;
static LED    : u32 = 26;

fn callback(_topic: &str, _payload: &str, _length: u32) {
    let val = digital_read(LED);
    //print(format!("Switch LED to {}\n", (val + 1) % 2).as_bytes());
    if val == HIGH {
        digital_write(LED, LOW);
    } else {
        digital_write(LED, HIGH);
    }
}

#[no_mangle]
pub fn main() {
    pin_mode(BUTTON, INPUT);
    pin_mode(LED, OUTPUT);

    sub_interrupt(BUTTON, callback, FALLING);

    loop {
        //delay(1);
    }
}

