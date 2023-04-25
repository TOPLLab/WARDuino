# Arduino platform for WARDuino

This folder contains the code for staging and flashing WARDuino with the `arduino-cli`.

To upload a program take the following steps:

1. compile your program to a `.wasm` WebAssembly binary
2. compile Arduino hexfile with:
```bash
make compile BINARY={{Path to .wasm file}} FQBN={{fqbn of target device}}
```
3. flash to device
```bash
make flash PORT={{serial port}} FQBN={{fqbn of target device}}
```

Alternatively you can pass the make arguments via a `.config` file that looks like this:

```make
PORT = /dev/ttyUSB0
FQBN = esp32:esp32:esp32wrover
PAUSED = true
BINARY = test.wasm
```

Place this file in the same directory as the `Makefile`, you can now just run `make compile`.

