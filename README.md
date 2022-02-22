[![compile](https://github.com/TOPLLab/WARDuino/actions/workflows/compile.yml/badge.svg)](https://github.com/TOPLLab/WARDuino/actions/workflows/compile.yml)
[![test](https://github.com/TOPLLab/WARDuino/actions/workflows/test.yaml/badge.svg)](https://github.com/TOPLLab/WARDuino/actions/workflows/test.yaml)

# WARDuino

[Build for Arduino](./README.md#toolchain-installation) | [Build CLI](./README.md#warduino-command-line-interface) | [Run Benchmarks](./README.md#run-the-benchmarks) | [Run Specification tests](./README.md#run-the-tests)

WARDuino is a port of the WebAssembly virtual machine for the ESP32/ESP8266 under the Arduino toolchain.

## ToolChain Installation

Follow these steps to execute the blink program on WARDuino on an ESP8266 or ESP32.

- Download [Arduino](https://www.arduino.cc/) and install it.

- Clone this repository under `$(HOME)/Arduino/libraries`.

```shell
git clone --recurse-submodules git@github.com:TOPLLab/WARDuino.git
cd WARDuino
```

- Add the `esp8266` or `esp32` driver depending on your board:

  - Add the repository:

    - File > Preferences > Settings > Additional board manager URLS

      ```
      http://arduino.esp8266.com/stable/package_esp8266com_index.json,https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
      ```

    - Or follow [these steps](https://github.com/esp8266/Arduino#installing-with-boards-manager) for esp8266 or [these instructions](https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md) for esp32

  - In the Arduino IDE, go to Tools -> Boards -> Boards Manager

    - Search for `esp8266` and install it (version 2.5.0)
    - Or search for `esp32` and install it (version 1.0.4)

- Restart Arduino.

- Install wat2wasm from the [`wabt`](https://github.com/WebAssembly/wabt) package

- Built the demo files.

```shell
cd demo/run_wa
make -C wa_sources/ all
make
```

- Upload the demo code to the board with the IDE (select the correct board, e.g. DOIT ESP32 DEVKIT V1), or from the command line:

```shell
BOARD=ESP32WROVER
../../scripts/upload $BOARD hello_world.ino
```

  - Use board `DOIT ESP32 DEVKIT V1`

## WARDuino Command-line Interface

- Clone the repository.

```shell
git clone --recurse-submodules git@github.com:TOPLLab/WARDuino.git
cd WARDuino
```

- Run cmake.

```shell
mkdir build
cd build
cmake ..
```

- Built the CLI

```shell
make warduino
```

## Run the benchmarks

- Install clang (version 13.0 or higher)

- Install wat2wasm from the [`wabt`](https://github.com/WebAssembly/wabt) package

- Clone the repository.

```shell
git clone --recurse-submodules git@github.com:TOPLLab/WARDuino.git
cd WARDuino
```

- Built the demo files.

```shell
cd demo/run_wa
make -C wa_sources/ all
make
cd ../../
```

- Run cmake.

```shell
mkdir build
cd build
cmake ..
```

- Built the benchmarks

```shell
make WARDuinoBench
```

## Run the tests 

```shell
pip install -r requirements.txt
./tests/integration/run_spec_tests.py --interpreter "build/warduino" --compiler "wat2wasm"
```

# LICENCE

MPL-2.0 (see LICENSE).

WARDuino by Robbert Gurdeep Singh, Christophe Scholliers and Joel Martin is licensed under a Mozilla Public License 2.0.
This is a derivative work of [kanaka/wac](https://github.com/kanaka/wac) by Joel Martin.

If you need to cite WARDuino in your research, use:

```bibtex
@inproceedings{WARDuino2019,
 author = {Gurdeep Singh, Robbert and Scholliers, Christophe},
 title = {WARDuino: A Dynamic WebAssembly Virtual Machine for Programming Microcontrollers},
 booktitle = {Proceedings of the 16th ACM SIGPLAN International Conference on Managed Programming Languages and Runtimes},
 series = {MPLR 2019},
 year = {2019},
 isbn = {978-1-4503-6977-0},
 location = {Athens, Greece},
 pages = {27--36},
 numpages = {10},
 url = {http://doi.acm.org/10.1145/3357390.3361029},
 doi = {10.1145/3357390.3361029},
 acmid = {3361029},
 publisher = {ACM},
 address = {New York, NY, USA},
 keywords = {Arduino, Live Code Updates, Virtual Machine, WebAssembly},
}
```
