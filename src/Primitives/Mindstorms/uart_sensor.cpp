#include "uart_sensor.h"

#include <cstdio>
#include <zephyr/drivers/uart.h>

static const struct device *const uart_dev =
        DEVICE_DT_GET(DT_PROP(DT_PATH(zephyr_user), warduino_uarts));

// Variables used for setting up the sensor.
int payload_bytes = 0;
int payload_index = 0;
unsigned int current_payload = 0;
unsigned char checksum;
bool data_byte = false;

// Variables that are used during setup but also by the heartbeat function.
int baudrate = -1;
uint8_t mode = 0;
volatile int sensor_value = 0;

volatile uint32_t receive_state = ReceiveState::advertise;
bool baudrate_configured = false;

void serial_cb([[maybe_unused]] const struct device *dev, [[maybe_unused]] void *user_data) {
    uint8_t data;

    if (!uart_irq_update(uart_dev)) {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev)) {
        return;
    }

    while (uart_fifo_read(uart_dev, &data, 1) == 1) {
        if (receive_state == ReceiveState::data) {
            if (data_byte) {
                sensor_value = data;
                data_byte = false;
            }

            // Check if it's a data message. This indicates the byte after this
            // will contain data.
            if (0b11000000 & data) {
                // Next byte will be data
                data_byte = true;
            }
            continue;
        }

        if (payload_bytes > 0) {
            payload_bytes--;

            if (payload_bytes > 1) {
                if (receive_state == ReceiveState::final_mode_format && payload_bytes == 5 && data != 0x80) {
                    receive_state = ReceiveState::advertise;
                    payload_bytes = 0;
                }
                checksum ^= data;
                current_payload = current_payload |
                                  (((unsigned long)data) << payload_index * 8);
                payload_index++;
            } else if (checksum == data) {
                if (receive_state == ReceiveState::advertise) {
                    printf("Baudrate = %d\n", current_payload);
                    baudrate = (int) current_payload;
                }
                else if (receive_state == ReceiveState::final_mode_format){
                    receive_state = ReceiveState::modes_complete;
                }
            }
        }

        // Handle ACK message.
        if (data == 0b00000100) {
            // If we receive an ACK after the final format message and we
            // know which speed to communicate at then we should send an ACK to
            // switch to data mode.
            if (receive_state == ReceiveState::modes_complete && baudrate > 0) {
                printf("Completing pairing sequence\n");
                uart_poll_out(uart_dev, 0b00000100);  // Send ACK back
                receive_state = ReceiveState::data;
            }
        }

        // Handle FORMAT commands.
        if (data >> 3 == 0b10010) {
            uint8_t sensor_mode = data & 0b111;
            printf("FORMAT for mode %d\n", sensor_mode);
            if (sensor_mode == 0) {
                receive_state = ReceiveState::final_mode_format;
                payload_bytes = 6;
                payload_index = 0;
                current_payload = 0;
                checksum = 0xff ^ data;
            }
        }

        // Handle speed command.
        if (data == 0b01010010) {
            payload_bytes = 5;
            payload_index = 0;
            current_payload = 0;
            checksum = 0xff ^ 0b01010010;
        }
    }
}

void set_sensor_mode(uint8_t new_mode) {
    uart_poll_out(uart_dev, 0x43);
    uart_poll_out(uart_dev, new_mode);
    uart_poll_out(uart_dev, 0xff ^ 0x43 ^ new_mode);
}

void uartHeartbeat() {
    if (receive_state != ReceiveState::data) {
        return;
    }

    if (!baudrate_configured) {
        printf("Changing baudrate to %d\n", baudrate);
        uart_config cfg{};
        uart_config_get(uart_dev, &cfg);
        cfg.baudrate = baudrate;

        int config_err = uart_configure(uart_dev, &cfg);
        printf("config_err = %d\n", config_err);
        if (config_err) {
            printf("UART configure error %d", config_err);
        }

        config_err = uart_config_get(uart_dev, &cfg);
        printf("current baudrate after config change = %d\n", cfg.baudrate);
        printf("config_err = %d\n", config_err);
        baudrate_configured = true;

        // Change to the desired mode.
        set_sensor_mode(mode);
    }

    if (baudrate_configured) {
        uart_poll_out(uart_dev, 0b00000010);
    }
}

bool sensor_ready() {
    return device_is_ready(uart_dev);
}

int get_sensor_value() {
    return sensor_value;
}

bool configure_uart_sensor(uint8_t new_mode) {
    if (!sensor_ready()) {
        printf("Input port is not ready!\n");
        return false;
    }

    printf("Setting up uart\n");
    int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb, nullptr);
    if (ret < 0) {
        if (ret == -ENOTSUP) {
            printf("Interrupt-driven UART API support not enabled\n");
        } else if (ret == -ENOSYS) {
            printf("UART device does not support interrupt-driven API\n");
        } else {
            printf("Error setting UART callback: %d\n", ret);
        }
        return false;
    }
    uart_irq_rx_enable(uart_dev);
    if (receive_state == ReceiveState::data && mode != new_mode) {
        set_sensor_mode(new_mode);
    }
    mode = new_mode;
    return true;
}
