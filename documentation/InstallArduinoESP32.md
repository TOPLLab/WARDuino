# Install instructions for Arduino ESP32

To use ESP32 boards with the WARDuino project you need to install the correct board manager for ESP32.

## Installing the board manager for ESP32

To use the ESP32 boards with `arduino-cli` perform the following steps:

1. Init the config file, if you have not done so yet.

```
arduino-cli config init
```

2. To find the location of your config file you can run:

```
arduino-cli config dump --verbose
```

3. Add the ESP32 board manager URL to the config file:

```
board_manager:
  additional_urls:
      - https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
```

4. Update index

```
arduino-cli core update-index
```

5. Install the ESP32 platform:

```
arduino-cli core install esp32:esp32
```

To use ESP32 boards with the WARDuino Project you need at least version 2.0.2 of the board manager.
you can check your version with:

```
arduino-cli core list
```

## Additional information

More information on how to use `arduino-cli` can be found [here](https://arduino.github.io/arduino-cli/0.21/getting-started/).

