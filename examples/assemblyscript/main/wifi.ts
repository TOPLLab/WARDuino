import {sleep, WiFi} from "as-warduino/assembly";
import * as config from "./config";

function until(attempt: () => void,
               done: () => boolean): void {
    while (!done()) {
        sleep(1);
        attempt();
    }
}

export function main(): void {
    // Connect to Wi-Fi
    until(
        () => { WiFi.connect(config.SSID, config.PASSWORD); },
        WiFi.connected);
    let message = "Connected to wifi network with ip: ";
    print(message.concat(WiFi.localip()));
   
    while (true) {
        sleep(5); // Sleep for 5 seconds
    }
}
