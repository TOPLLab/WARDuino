import {Serial, Setup, Radio, M5Cardputer} from "as-warduino/assembly";
import { delay } from "../package/assembly";

let initialized: boolean = false;

function setup(): void {
    Serial.begin(115200);

    M5Cardputer.begin(); 
    M5Cardputer.display_setRotation(1);
    M5Cardputer.display_fillScreen(0x000000);
    M5Cardputer.display_setTextColor(0x7e0);
    M5Cardputer.display_setTextSize(2);
    M5Cardputer.display_setCursor(10, 10);
    M5Cardputer.display_println("LoRa Sender");
    M5Cardputer.display_println("Waiting...");
    delay(1000);
    
    let state: number = Radio.begin(915, 125, 9, 7, 0x12, 17);

    if (state != 0) {
        M5Cardputer.display_println("Initialization failed: ");
        M5Cardputer.display_print("Error: ");
        M5Cardputer.display_println(state.toString());
        while (true) delay(10000);
    }
        
    M5Cardputer.display_println("SX1262 ready - Sender");
}

export function main(): void {
    if (Setup.check_initialised() == false) {
        setup();
        Setup.set_initialised();
    }

    M5Cardputer.display_println("Sending...");
    let state: number = Radio.transmit("Hello from Sender!");

    if (state == 0) {
        M5Cardputer.display_println("Success!");
    } else {
        M5Cardputer.display_println("Fail!");
        M5Cardputer.display_println(state.toString());
        M5Cardputer.display_println(state.toString());
    }

    delay(2000);
}