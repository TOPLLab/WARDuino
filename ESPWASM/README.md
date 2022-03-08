# WARDuino: Build and Development Instructions

The project uses CMake. Quick install looks like this:

```bash
git clone git@github.com:TOPLLab/WARDuino.git
cd WARDuino/ESPWASM
mkdir build
cd build
cmake ..
make
```

This will build the command-line tool (EMULATOR) by default.

## Build for ESP-IDF

To install the WARDuino with the ESP-IDF toolchain perform the following steps starting from the project root folder:

```bash
cd ESPWASM
mkdir build-esp
cd build-esp
cmake ..
make flash
```

## Build for Arduino

To install the WARDuino with the Arduino toolchain perform the following steps starting from the project root folder:

## Build for Emulator

WARDuino can also be build as a command-line tool for a desktop environment.
The purpose of this CLI is to allow developers to test WARDuino applications without the need to buy a microcontroller.
The CLI is also used to run the various unit and specification tests for WARDuino.

To install the CLI perform the following steps starting from the project root folder:

```bash
cd ESPWASM
mkdir build
cd build
cmake ..
make
```

## Unit testing


## WebAssembly Specification tests


## Technical support and feedback

For a feature request or bug report, create a [GitHub issue](https://github.com/TOPLLab/WARDuino/issues).

