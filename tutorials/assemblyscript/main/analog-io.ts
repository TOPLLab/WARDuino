import {analogRead,
        analogWrite,
        delay,
        print} from "as-warduino/assembly";

const IN: u32 = 13;
const OUT: u32 = 9;

function map(value: u32, x1: u32, y1: u32, x2: u32, y2: u32): u32 {
    return <u32>((value - x1) * (y2 - x2) / (y1 - x1) + x2);
}

export function main(): void {
    const value = analogRead(IN);

    const output = map(value, 0, 1023, 0, 255);

    analogWrite(OUT, output);

    print(`sensor = ${value}\t output = ${output}`);
    delay(1);
}
