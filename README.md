<div align="center">
  <h1>WARDuino</h1>

  <p>
    <strong>Wasm virtual machine for ESP32 / Arduino</strong>
  </p>

  <p>
    <a href="https://github.com/TOPLLab/WARDuino/actions/workflows/compile.yml"><img src="https://github.com/TOPLLab/WARDuino/actions/workflows/compile.yml/badge.svg"></a>
    <a href="https://github.com/TOPLLab/WARDuino/actions/workflows/test.yml"><img src="https://github.com/TOPLLab/WARDuino/actions/workflows/test.yml/badge.svg"></a>
    <a href="https://github.com/TOPLLab/WARDuino/blob/master/LICENSE"><img src="https://img.shields.io/badge/License-MPL_2.0-blue.svg"></a>
  </p>

  <b>
    <a href="./README.md#build-and-development-instructions">Installation</a>
    <span> | </span>
    <a href="./tutorials/">Examples</a>
    <span> | </span>
    <a href="./README.md#webassembly-specification-tests">Run Specification tests</a>
    <span> | </span>
    <a href="https://topllab.github.io/WARDuino/guide/get-started.html">Documentation</a>
  </b>

</div>

## About

This project is released under the Mozilla Public License 2.0, and is being developed as part of an active research project at the University of Ghent's [TOPL Lab](https://github.com/TOPLLab).

+ The WARDuino virtual machine is a WebAssembly runtime for microcontrollers, which runs both under the Arduino and ESP-IDF toolchains.
+ The WARDuino virtual machine features an extensive debugger with novel techniques such as out-of-place debugging.
+ The virtual machine allows developers to implement their own primitives in C++, which are callable from Wasm.
+ The WARDuino project also includes a [VS Code extension](https://github.com/TOPLLab/WARDuino-VSCode) to use both the remote debugging and the out-of-place debugging facilities offered by the virtual machine.

> [!WARNING]
> WARDuino is not 1.0, since this is an active research project. Expect possible bugs or performance issues.

## Build and Development Instructions

> [!NOTE]
> **Supported platforms:** Linux (Ubuntu), macOS, ESP-IDF, Arduino

The project uses CMake. Quick install looks like this:

```bash
git clone --recursive git@github.com:TOPLLab/WARDuino.git
cd WARDuino
mkdir build-emu
cd build-emu
cmake .. -D BUILD_EMULATOR=ON
make
```

This will build the command-line tool (`emulator`), which has been tested on both linux and macOS.

The WARDuino VM can be compiled with both the Arduino and ESP-IDF toolchains, and has been extensively tested on different ESP8266 and ESP32 microcontrollers.

### Build for ESP-IDF

> [!WARNING]
> Primitive support for IDF is under construction.

Before you can compile and flash with ESP-IDF, you must install and enable [the toolchain](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html).
You also need to disable the watchdog timer:

1. Go to the root folder of the WARDuino repo
2. run `idf.py menuconfig`
3. Under **Component config → ESP System Settings** disable the following options:
   - Interrupt watchdog
   - Initialize Task Watchdog Timer on startup
4. Save and quit the menu

Make sure the ESP-IDF tools are enabled, otherwise these steps will not work.

To install the WARDuino with the ESP-IDF toolchain perform the following steps starting from the project root folder:

```bash
mkdir build
cd build
cmake .. -D BUILD_ESP=ON
make flash
```

Or simply run `idf.py flash`.

### Build for Arduino

First, install the [arduino-cli](https://arduino.github.io/arduino-cli/0.21/installation/).
You will also need python3 with the pyserial pacakge.

Second, create the config file:

```bash
arduino-cli config init
```

If you need additional boards, such as the esp32 boards, you can add them in the generated config file. More information on how to install the esp32 boards can be found <a href="./documentation/InstallArduinoESP32.md">here</a>.
(_note: WARDuino requires at least version 2.0.2 of the esp32 board manager when using esp32 devices)_

Thirdly, make sure you install the `PubSubClient` and `Adafruit NeoPixel` library. (used for MQTT and pixel primitives)

```bash
arduino-cli lib install "PubSubClient" # for MQTT
arduino-cli lib install "Adafruit NeoPixel" # for some primitives
```

To build for Arduino with WIFI support you need to also install the following third-party libraries.
(You might need to set `enable_unsafe_install` to `true` in your arduino config)

```bash
arduino-cli lib install FreeRTOS
arduino-cli lib install --git-url https://github.com/me-no-dev/AsyncTCP.git
```

If you haven't done so already, clone (or symlink) this repository to `~/Arduino/libraries` to make WARDuino availible to Arduino.


After this initial installation steps you can start using WARDuino with the Arduino toolchain.
You can upload the example file as follows, starting from the project root:

```bash
cd platforms/Arduino
make compile
make flash
```

### Build for Emulator

WARDuino can also be build as a command-line tool for a desktop environment.
The purpose of this CLI is to allow developers to test WARDuino applications without the need to buy a microcontroller.
The CLI is also used to run the various unit and specification tests for WARDuino.

To install the CLI perform the following steps starting from the project root folder:

```bash
mkdir build-emu
cd build-emu
cmake .. -D BUILD_EMULATOR=ON
make
```

## WebAssembly Specification tests

```shell
cd tests/latch
npm run spectest
```

## Technical support and feedback

For a feature request or bug report, create a [GitHub issue](https://github.com/TOPLLab/WARDuino/issues).

## Acknowledgments

WARDuino by Robbert Gurdeep Singh, Tom Lauwaerts, Carlos Rojas Castillo, and Christophe Scholliers is licensed under a [MPL-2.0 License](./LICENSE).
This is a derivative work of [kanaka/wac](https://github.com/kanaka/wac) by Joel Martin.

If you need to cite WARDuino in your research, use:

```bibtex
@article{	  WARDuino2024,
  title		= {WARDuino: An Embedded WebAssembly Virtual Machine},
  shorttitle	= {WARDuino},
  author	= {Lauwaerts, Tom and Gurdeep Singh, Robbert and Scholliers,
		  Christophe},
  year		= {2024},
  month		= feb,
  journal	= {Journal of Computer Languages},
  pages		= {101268},
  issn		= {2590-1184},
  doi		= {10.1016/j.cola.2024.101268},
  keywords	= {Internet-of-Things,Language symbiosis,Virtual
		  machine,WARDuino,WebAssembly}
}
```

