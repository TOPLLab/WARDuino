import {digitalRead, digitalWrite, InterruptMode, MQTT, pinMode,
        PinMode, PinVoltage, print, sleep, WiFi} from "as-warduino/assembly";
import * as config from "./config";

function until(attempt: () => void,
               done: () => boolean): void {
    while (!done()) {
        sleep(1);
        attempt();
    }
}

function callback(topic: string,
                  payload: string): void {
    print("Message [" + topic + "] " + payload + "\n");

    if (payload.includes("on")) {
        digitalWrite(config.LED, PinVoltage.HIGH);  // Turn the LED on
    } else {
        digitalWrite(config.LED, PinVoltage.LOW);   // Turn the LED off
    }
}

function toggleLed(): void {
    // Get current status of LED
    let status = digitalRead(config.LED);
    // Toggle LED
    digitalWrite(config.LED, !status);
}

export function main(): void {
    // Set pin modes
    pinMode(config.LED, PinMode.OUTPUT);
    pinMode(config.BUTTON, PinMode.INPUT);

    // Connect to Wi-Fi
    until(
        () => { WiFi.connect(config.SSID, config.PASSWORD); },
        WiFi.connected);
    let message = "Connected to wifi network with ip: ";
    print(message.concat(WiFi.localip()));

    // Connect to MQTT broker
    MQTT.init("192.168.0.24", 1883);
    until(
        () => { MQTT.connect(config.CLIENT_ID); MQTT.loop(); },
        MQTT.connected);

    // Subscribe to MQTT topic and turn on LED
    MQTT.subscribe("LED", callback);
    MQTT.publish("LED", "on");

    // Subscribe to button interrupt
    interruptOn(config.BUTTON, InterruptMode.CHANGED, toggleLed);
   
    while (true) {
        until(
            () => { MQTT.connect(config.CLIENT_ID); MQTT.loop(); },
            MQTT.connected);

        sleep(5); // Sleep for 5 seconds
    }
}
