import {analogRead, delay, print} from "as-warduino/assembly";

export function main(): void {
    const value = analogRead(13);
    print(value);
    delay(1);
}
