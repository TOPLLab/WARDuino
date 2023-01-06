# Examples in Textual WebAssembly

The `main` folder contains example programs written in textual WebAssembly.

## Compiling and Running the Examples

To compile and flash to an ESP32 with the ESP-IDF toolchain simply run the build script:

```bash
bash ./build.sh blink
```

The script takes the name of a wat file from the main folder as argument (without extension).

## Blink example

The `blink.wat` file implements the traditional "blinking LED" example for microcontrollers in WebAssembly Text Format (WAT).

## Factorial example

The `fac.wat` file implements the traditional wasm example. The program prints `5!` to the serial port in an infinite loop.

This is also an interesting example to run with the commandline interface:

```bash
wdcli fac.wasm --invoke fac 10
```

