import * as wd from "../lib/warduino";

const BUTTON = 25;
const LED = 26;
const SSID = "local-network";
const PASSWORD = "network-password";
const CLIENT_ID = "random-mqtt-client-id";

function until_connected(connect: () => void,
                         connected: () => boolean,
                         retry: () => boolean): void {
    connect();
    while (!connected()) {
        wd.delay(1000);
        if (retry) connect();
    }
}

function callback(topic: string,
                  payload: string): void {
    wd.print("Message [" + topic + "] " + payload + "\n");

    if (payload.includes("on")) {
        wd.digital_write(LED, wd.HIGH);  // Turn the LED on
    } else {
        wd.digital_write(LED, wd.LOW);   // Turn the LED off
    }
}

function toggle_led(): void {
    // Get current status of LED
    let status = wd.digital_read(LED);
    // Toggle LED
    wd.digital_write(LED, !status);
}

export function main(): void {
    // Set pin modes
    wd.pin_mode(LED, wd.OUTPUT);
    wd.pin_mode(BUTTON, wd.INPUT);

    // Connect to Wi-Fi
    until_connected(
        () => { wd.wifi_connect(SSID, PASSWORD); },
        wd.wifi_connected,
        () => { return wd.wifi_status() === 6; });
    let message = "Connected to wifi network with ip: ";
    wd.print(message.concat(wd.wifi_localip()));

    // Connect to MQTT broker
    wd.mqtt_init("192.168.0.24", 1883);
    until_connected(
        () => { wd.mqtt_connect(CLIENT_ID); wd.mqtt_loop(); },
        () => { return wd.mqtt_connected(); },
        () => { return true; });

    // Subscribe to MQTT topic and turn on LED
    wd.mqtt_subscribe("LED", callback);
    wd.mqtt_publish("LED", "on");

    // Subscribe to button interrupt
    wd.interrupt_on(BUTTON, wd.CHANGED, toggle_led);
   
    while (true) {
        until_connected(
            () => { wd.mqtt_connect(CLIENT_ID); wd.mqtt_loop(); },
            () => { return wd.mqtt_connected(); },
            () => { return true; });

        wd.sleep(5); // Sleep for 5 seconds
    }
}
