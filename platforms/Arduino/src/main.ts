// Blinking LED example
@external("env", "chip_pin_mode")       export declare function pinMode(pin: u32, mode: u32): void;
@external("env", "chip_digital_write")  export declare function digitalWrite(pin: u32, value: u32): void;
@external("env", "chip_delay")          export declare function delay(ms: u32): void;
@external("env", "display_init")        export declare function initDisplay(): void;
@external("env", "display_draw_rect")   export declare function displayDrawRect(x:i32,y:i32,w:i32,h:i32): void;

enum PinVoltage {
    /** Low voltage on a digital I/O pin */
    LOW  = 0,
    /** High voltage on a digital I/O pin */
    HIGH = 1,
}


export function main(): void {
    const led: u32 = 4;
    const pause: u32 = 1000;
				const OUTPUT: u32 = 3;
				let x:i32 = 0;

    pinMode(led, OUTPUT);
				initDisplay();


    while (true) {
        digitalWrite(led, PinVoltage.HIGH);
        delay(pause);
        digitalWrite(led, PinVoltage.LOW);
								delay(pause);
								displayDrawRect(x,0,150,150);
								x = x+10;
				}
}
