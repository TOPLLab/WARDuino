import {Serial, Radio, M5Cardputer} from "as-warduino/assembly";
import { delay, print } from "../package/assembly";

Serial.begin(115200);

M5Cardputer.begin(); 
M5Cardputer.display_setRotation(1);
M5Cardputer.display_fillScreen(0x000000);
M5Cardputer.display_setTextColor(0xFFFFFF);
M5Cardputer.display_setTextSize(2);
M5Cardputer.display_setCursor(10, 10);
M5Cardputer.display_println("LoRa Receiver");
M5Cardputer.display_println("Waiting...");
delay(1000);

let state: number = Radio.begin(915, 125, 9, 7, 0x12, 17);

if (state != 0) {
    M5Cardputer.display_println("Initialization failed: ");
    M5Cardputer.display_print("Error: ");
    M5Cardputer.display_println(state.toString());
    while (true) delay(10000);
}   

M5Cardputer.display_println("SX1262 ready - Receiver");
Radio.startReceive();

export function main(): void {
    print("receiver main loop");
    
    let buffer = new Uint8Array(128);
    let state: number = Radio.receive(buffer);

    if (state == 0) {
        let msg = String.UTF8.decode(buffer, true);
        M5Cardputer.display_fillScreen(0);
        M5Cardputer.display_setCursor(10, 10);
        M5Cardputer.display_setTextColor(0x7e0);
        M5Cardputer.display_setTextSize(2);
        M5Cardputer.display_println("Received:");
        M5Cardputer.display_println(msg);
    }

}
