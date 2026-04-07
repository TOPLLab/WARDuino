/**
 * as-warduino - WARDuino primitives library
 *
 * <br>
 *
 * WARDuino is a dynamic WebAssembly runtime for embedded devices, that
 * provides primitives to access hardware and IoT specific functionality.
 *
 * The `as-warduino` package allows using these primitives from AssemblyScript.
 *
 * <br>
 *
 * # Usage
 *
 * ```ts
 * import {pinMode, PinMode, PinVoltage, digitalWrite, delay} from "as-warduino";
 * 
 * export function main(): void {
 *     let led = 26;
 *     pinMode(led, PinMode.OUTPUT);
 * 
 *     let pause: u32 = 1000;
 *     while (true) {
 *         digitalWrite(led, PinVoltage.HIGH);
 *         delay(pause);
 *         digitalWrite(led, PinVoltage.LOW);
 *         delay(pause);
 *     }
 * }
 * ```
 */

import * as ward from "./warduino";

/** Returns the number of milliseconds passed since the current program started to run. */
export function millis(): u32 {
    return ward._millis();
}
/** Pauses the program for the amount of time (in milliseconds). */
export function delay(ms: u32): void {
    ward._delay(ms);
}

/** Pauses the program for the amount of time (in seconds). */
export function sleep(s: u32): void {
    ward._delay(s * 1000);
}

/** Reads the value from the specified analog pin. */
export function analogRead(pin: u32): i32 {
    return ward._analog_read(pin);
}

/** Write the value to the specified analog pin. */
export function analogWrite(pin: u32, signal: u32): void {
    ward._analog_write(pin, signal);
}

export function analogSetup(channel: u32, frequency: u32, timer: u32): void {
    ward._analog_setup(channel, frequency, timer);
}

export function analogAttach(pin: u32, channel: u32): void {
    ward._analog_attach(pin, channel);
}

export function analogDuty(channel: u32, value: i32, max: u32): void {
    ward._analog_duty(channel, value, max);
}

/** The voltage of a digital pin. */
export enum PinVoltage {
    /** Low voltage on a digital I/O pin */
    LOW  = 0,
    /** High voltage on a digital I/O pin */
    HIGH = 1,
}

/** The mode of a pin interrupt. */
export enum InterruptMode {
    /** Changing edge on a digital I/O pin */
    CHANGED = 1,
    /** Falling edge on a digital I/O pin */
    FALLING = 2,
    /** Rising edge on a digital I/O pin */
    RISING  = 3,
}

/** The mode of a digital I/O pin. */
export enum PinMode {
    /** Input mode for digital pins */
    INPUT  = 0x0,
    /** Output mode for digital pins */
    OUTPUT = 0x2,
}

/** Configures the [PinMode] of the specified pin. */
export function pinMode(pin: u32, mode: PinMode): void {
    ward._pin_mode(pin, mode);
}

/** Write the voltage to a specified digital pin, either [HIGH](PinVoltage) or [LOW](PinVoltage). */
export function digitalWrite(pin: u32, value: PinVoltage): void {
    ward._digital_write(pin, value);
}

/** Reads the value from a specified digital pin, either [HIGH](PinVoltage) or [LOW](PinVoltage). */
export function digitalRead(pin: u32): PinVoltage {
    return ward._digital_read(pin);
}

/** Subscribe a callback function to an interrupt on the given pin. */
export function interruptOn(pin: u32, mode: InterruptMode, fn: (topic: string, payload: string) => void): void {
    ward._subscribe_interrupt(pin, fn, mode);
}

/** Print a string to the serial port. */
export function print(text: string): void {
    ward._print(String.UTF8.encode(text, true), String.UTF8.byteLength(text, true));
}

export namespace WiFi {
    export enum Status {
        /** No Wi-Fi hardware found */
        NoShield = 255,
        /** Wi-Fi is in process of changing between statuses */
        Idle = 0,
        /** Configured SSID cannot be reached */
        SsidUnavailable = 1,
        /** */
        ScanCompleted = 2,
        /** Successful connection is established */
        Connected = 3,
        /** Failed to connect */
        ConnectFailed = 4,
        /** No longer connected */
        ConnectionLost = 5,
        /** Module is not configured in station mode */
        Disconnected = 6,
    }

    /** Connect to Wi-Fi network with SSID and password. */
    function connect(ssid: string, password: string): void {
        ward._wifi_connect(String.UTF8.encode(ssid, true), String.UTF8.byteLength(ssid, true),
                      String.UTF8.encode(password, true), String.UTF8.byteLength(password, true));
    }

    /** Returns the status of the Wi-Fi connection of the board. */
    function status(): Status {
        return ward._wifi_status();
    }

    /** Returns whether the board si still connected to Wi-Fi. */
    function connected(): bool {
        return status() === Status.Connected;
    }

    /** Returns the local IP address of the board. */
    function localip(): string {
        let localip = new ArrayBuffer(20);
        ward._wifi_localip(localip, localip.byteLength);
        return String.UTF8.decode(localip, true);
    }
}

export namespace MQTT {
    export enum Status {
        /** The server didn't respond within the keepalive time */
        ConnectionTimeout = -4,
        /** The network connection was broken */
        ConnectionLost = -3,
        /** The network connection failed */
        ConnectFailed = -2,
        /** The client is disconnected cleanly */
        Disconnected = -1,
        /** The client is connected */
        Connected = 0,
        /** the server doesn't support the requested version of MQTT */
        ConnectBadProtocol = 1,
        /** The server rejected the client identifier */
        ConnectBadClientId = 2,
        /** The server was unable to accept the connection */
        ConnectUnavailable = 3,
        /** The username/password were rejected */
        ConnectBadCredentials = 4,
        /** The client was not authorized to connect */
        ConnectUnauthorized = 5,
    }

    /**  Configure an MQTT broker. */
    function configureBroker(server: string, port: u32): void {
        ward._mqtt_init(String.UTF8.encode(server, true), String.UTF8.byteLength(server, true), port);
    }

    /** Connect to the Configured MQTT broker with client_id. */
    function connect(client_id: string): i32 {
        return ward._mqtt_connect(String.UTF8.encode(client_id, true), String.UTF8.byteLength(client_id, true));
    }

    /** Returns whether the board is still connected to the MQTT broker. */
    function connected(): bool {
        return status() === Status.Connected;
    }

    /** Returns the status of the connection to the MQTT broker. */
    function  status(): Status {
        return ward._mqtt_state();
    }

    /** Publish a message on an MQTT topic. */
    function publish(topic: string, payload: string): i32 {
        return ward._mqtt_publish(String.UTF8.encode(topic, true), String.UTF8.byteLength(topic, true), String.UTF8.encode(payload, true), String.UTF8.byteLength(payload, true));
    }

    /** Subscribe a callback function to an MQTT topic. */
    function  subscribe(topic: string, fn: (topic: string, payload: string) => void): i32 {
        return ward._mqtt_subscribe(topic, fn);
    }

    /** Unsubscribe a callback function from an MQTT topic. */
    function  unsubscribe(topic: string, fn: (topic: string, payload: string) => void): i32 {
        return ward._mqtt_unsubscribe(topic, fn);
    }

    /**  Check for messages from the MQTT broker. */
    function  loop(): i32 {
        return ward._mqtt_loop();
    }
}

export namespace Setup {

    export function check_initialised(): boolean {
        return ward._check_initialised();
    }

    export function set_initialised(): void {
        ward._set_initialised();
    }
}

const buffersizeMaximum: u32 = 1000;
const radioBuffer: ArrayBuffer = new ArrayBuffer(buffersizeMaximum);

export namespace Radio {

    export function begin(frequency: i32, bandwidth: i32, spreadingFactor: i32, codingRate: i32, syncWord: i32, outputPower: i32): u32 {
        return ward._radio_begin(frequency, bandwidth, spreadingFactor, codingRate, syncWord, outputPower);
    }

    export function transmit(data: string): i32 {
        return ward._radio_transmit(String.UTF8.encode(data, true), String.UTF8.byteLength(data, true));
    }

    export function startReceive(): i32 {
        return ward._radio_startReceive();
    }

    export function receive(): i32 {
        return ward._radio_receive(radioBuffer, radioBuffer.byteLength);
    }

    export function getReceivedMessage(): string {
        return String.UTF8.decode(radioBuffer, true);
    }
}

export namespace Serial {
    export function begin(baud_rate: i32): void {
        ward._serial_begin(baud_rate);
    }
}
    

export namespace M5Cardputer {
    export function begin(): void {
        ward._m5_cardputer_begin();
    }

    export function display_setRotation(rotation: i32): void {
        ward._m5_cardputer_display_setRotation(rotation);
    }

    export function display_fillScreen(color: i32): void {
        ward._m5_cardputer_display_fillScreen(color);
    }

    export function display_setTextColor(color: i32): void {
        ward._m5_cardputer_display_setTextColor(color);
    }

    export function display_setTextSize(size: i32): void {
        ward._m5_cardputer_display_setTextSize(size);
    }

    export function display_setCursor(x: i32, y: i32): void {
        ward._m5_cardputer_display_setCursor(x, y);
    }

    export function display_println(text: string): i32 {
        return ward._m5_cardputer_display_println(String.UTF8.encode(text, true), String.UTF8.byteLength(text, true));
    }

    export function display_print(text: string): i32 {
        return ward._m5_cardputer_display_print(String.UTF8.encode(text, true), String.UTF8.byteLength(text, true));
    }
}

export namespace HTTP {
    class PostOptions {
        url: string;
        body: string;
        content_type: string;
        authorization: string;

        constructor(url: string, body: string, content_type: string, authorization: string) {
            this.url = url; this.body = body; this.content_type = content_type; this.authorization = authorization;
        }
    }

    /** Send an HTTP GET request. The response is written to an ArrayBuffer. */
    function get(url: string): string {
        const response: ArrayBuffer = new ArrayBuffer(buffersizeMaximum);
        ward._http_get(String.UTF8.encode(url, true), String.UTF8.byteLength(url, true), response, response.byteLength);
        return String.UTF8.decode(response);
    }

    /** Send an HTTP POST request. The response is written to an ArrayBuffer. */
    function post(options: PostOptions): string {
        const response: ArrayBuffer = new ArrayBuffer(buffersizeMaximum);
        ward._http_post(String.UTF8.encode(options.url, true), String.UTF8.byteLength(options.url, true),
                          String.UTF8.encode(options.body, true), String.UTF8.byteLength(options.body, true),
                          String.UTF8.encode(options.content_type, true), String.UTF8.byteLength(options.content_type, true),
                          String.UTF8.encode(options.authorization, true), String.UTF8.byteLength(options.authorization, true),
                          response, response.byteLength);
        return String.UTF8.decode(response);
    }
}
