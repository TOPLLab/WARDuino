[![compile](https://github.com/TOPLLab/WARDuino/actions/workflows/compile.yml/badge.svg)](https://github.com/TOPLLab/WARDuino/actions/workflows/compile.yml)
[![test](https://github.com/TOPLLab/WARDuino/actions/workflows/test.yaml/badge.svg)](https://github.com/TOPLLab/WARDuino/actions/workflows/test.yaml)

# WARDuino

## Build and Development Instructions

The project uses CMake. Quick install looks like this:

```bash
git clone git@github.com:TOPLLab/WARDuino.git
cd WARDuino
mkdir build-emu
cd build-emu
cmake .. -D BUILD_EMULATOR=ON
make
```

This will build the command-line tool (EMULATOR).

### Build for ESP-IDF

Before you can compile and flash with ESP-IDF, you must install and enable the toolchain.
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

Second, create the config file:

```bash
arduino-cli config init
```

If you need additional boards, such as the esp32 boards, you can add them in the generated config file. More information [here](https://arduino.github.io/arduino-cli/0.21/getting-started/).

Thirdly, make sure you install the `PubSubClient` library. (used for MQTT)

```bash
arduino-cli lib install PubSubClient
```

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

### WebAssembly Specification tests

```shell
python3 -m pip install -r requirements.txt
./tests/integration/run_spec_tests.py --interpreter "build-emu/wdcli" --compiler "wat2wasm"
```

### Technical support and feedback

For a feature request or bug report, create a [GitHub issue](https://github.com/TOPLLab/WARDuino/issues).

## LICENCE

MPL-2.0 (see LICENSE).

WARDuino by Robbert Gurdeep Singh, Tom Lauwaerts, Christophe Scholliers and Joel Martin is licensed under a Mozilla Public License 2.0.
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

