# WARDuino

Warduino is a port of the Web assembly virtual machine for the ESP32/ESP8266 under the arduino toolchain.

# ToolChain Installation

Follow these steps to execute the blink program on WARduino on an ESP8266 or ESP32.

- Download [Arduino](https://www.arduino.cc/) and install it.

- Setup `emcc` version v.1.38.47 or higher.

- Clone this repository under `$(HOME)/Arduino/libraries`.

- Add the `esp8266` or `esp32` driver depending on your board:

  - Add the repository:

    - File > Preferences > Settings > Additional board manager URLS

      ```
      http://arduino.esp8266.com/stable/package_esp8266com_index.json,https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
      ```

    - Or follow the steps at https://github.com/esp8266/Arduino#installing-with-boards-manager for esp8266 or https://github.com/espressif/arduino-esp32/blob/master/docs/arduino-ide/boards_manager.md for esp32

  - Go to Tools->Boards->Boards Manager

    - Search for `esp8266` and install it (version 2.5.0)
    - Or search for `esp32` and install it (version 1.0.4)

- Restart Arduino.

- Go to the `demo/run_wa` folder.

- Execute `make -C wa_sources/ all`.

- Execute `make up` or use the Arduino IDE to compile and upload the `.ino` file.

  - Use board `DOIT ESP32 DEVKIT V1`

## Run WARDuino on host machine

- Install wat2wasm from the [`wabt`](https://github.com/WebAssembly/wabt) package


## Run the tests 

- mkdir build_test 
- cd build_test 
- cmake ../ 
- make TestWARDuino 
- ./TestWarDuino <path-to-test-files>

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
