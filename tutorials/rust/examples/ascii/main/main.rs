use warduino::{print};

#[no_mangle]
pub fn main() {
    print("ASCII Table ~ Character Map\n");

    for byte in 33..=125 {
        print(&format!("{}, dec: {}, hex: {:x}, oct: {:o}, bin: {:b}\n", char::from_u32(byte).unwrap(), byte, byte, byte, byte).to_string());
    }
}
