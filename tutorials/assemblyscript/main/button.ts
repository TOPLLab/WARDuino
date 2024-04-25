import {
    delay,
    digitalRead,
    digitalWrite,
    InterruptMode,
    interruptOn,
    pinMode,
    PinMode,
    PinVoltage
} from "as-warduino/assembly";

const button = 25;
const led = 26;

function invert(voltage: PinVoltage): PinVoltage {
    switch (voltage) {
        case PinVoltage.LOW:
            return PinVoltage.HIGH;
        case PinVoltage.HIGH:
        default:
            return PinVoltage.LOW;
    }
}

function toggleLED(_topic: string, _payload: string): void {
    // Get current status of LED
    let status = digitalRead(led);
    // Toggle LED
    digitalWrite(led, invert(status));
}

export function main(): void {
    pinMode(button, PinMode.INPUT);
    pinMode(led, PinMode.OUTPUT);

    interruptOn(button, InterruptMode.FALLING, toggleLED);

    while (true) {
        delay(1000);
    }
}
