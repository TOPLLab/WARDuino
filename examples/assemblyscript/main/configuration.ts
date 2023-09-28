import {pinMode, PinMode} from "as-warduino/assembly";
import * as config from "./config";

export function main(): void {
    let led = config.LED;
    pinMode(led, PinMode.OUTPUT);
}
