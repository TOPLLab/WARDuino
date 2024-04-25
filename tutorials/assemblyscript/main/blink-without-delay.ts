// Blinking LED example
import {pinMode, PinMode, PinVoltage,
        digitalWrite, delay} from "as-warduino/assembly";

export function main(): void {
    const led = 26;
    const pause: u32 = 1000;

    pinMode(led, PinMode.OUTPUT);

    while (true) {
        digitalWrite(led, PinVoltage.HIGH);
        delay(pause);
        digitalWrite(led, PinVoltage.LOW);
        delay(pause);
    }
}
