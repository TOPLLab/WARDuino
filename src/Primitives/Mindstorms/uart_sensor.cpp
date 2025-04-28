#ifndef ARDUINO
#include "uart_sensor.h"

#include <zephyr/drivers/uart.h>

#include <cstdio>

void serial_cb(const struct device *dev, void *user_data) {
    auto *sensor = static_cast<UartSensor *>(user_data);
    uint8_t data;

    if (!uart_irq_update(dev)) {
        return;
    }

    if (!uart_irq_rx_ready(dev)) {
        return;
    }

    while (uart_fifo_read(dev, &data, 1) == 1) {
        // printf("data: 0x%02x %c\n", data, data);

        if (sensor->receive_state == ReceiveState::data) {
            if (sensor->data_byte) {
                sensor->sensor_value = data;
                sensor->data_byte = false;
            }
            // Check if it's a data message. This indicates the byte after this
            // will contain data.
            else if (data >> 6 == 0b11) {
                // Next byte will be data
                sensor->data_byte = true;
            }
            continue;
        }

        if (sensor->payload_bytes > 0) {
            sensor->payload_bytes--;

            if (sensor->payload_bytes > 1) {
                if (sensor->receive_state == ReceiveState::final_mode_format &&
                    sensor->payload_bytes == 5 && data != 0x80) {
                    sensor->receive_state = ReceiveState::advertise;
                    sensor->payload_bytes = 0;
                }
                sensor->checksum ^= data;
                sensor->current_payload =
                    sensor->current_payload |
                    (((unsigned long)data) << sensor->payload_index * 8);
                sensor->payload_index++;
            } else if (sensor->checksum == data) {
                if (sensor->receive_state == ReceiveState::advertise) {
                    printf("Baudrate = %d\n", sensor->current_payload);
                    sensor->baudrate = (int)sensor->current_payload;
                } else if (sensor->receive_state ==
                           ReceiveState::final_mode_format) {
                    sensor->receive_state = ReceiveState::modes_complete;
                }
            }
        }

        // Handle ACK message.
        if (data == 0b00000100) {
            // If we receive an ACK after the final format message and we
            // know which speed to communicate at then we should send an ACK to
            // switch to data mode.
            if (sensor->receive_state == ReceiveState::modes_complete &&
                sensor->baudrate > 0) {
                printf("Completing pairing sequence\n");
                uart_poll_out(dev, 0b00000100);  // Send ACK back
                sensor->receive_state = ReceiveState::data;
            }
        }

        // Handle FORMAT commands.
        if (data >> 3 == 0b10010) {
            uint8_t sensor_mode = data & 0b111;
            printf("FORMAT for mode %d\n", sensor_mode);
            if (sensor_mode == 0) {
                sensor->receive_state = ReceiveState::final_mode_format;
                sensor->payload_bytes = 6;
                sensor->payload_index = 0;
                sensor->current_payload = 0;
                sensor->checksum = 0xff ^ data;
            }
        }

        // Handle speed command.
        if (data == 0b01010010) {
            sensor->payload_bytes = 5;
            sensor->payload_index = 0;
            sensor->current_payload = 0;
            sensor->checksum = 0xff ^ 0b01010010;
        }
    }
}

void set_sensor_mode(UartSensor *sensor, uint8_t new_mode) {
    uart_poll_out(sensor->dev, 0x43);
    uart_poll_out(sensor->dev, new_mode);
    uart_poll_out(sensor->dev, 0xff ^ 0x43 ^ new_mode);

    // Invalidate current sensor values. This prevents the program reading
    // values that it normally cannot read in a particular mode.
    sensor->sensor_value = -1;
}

bool configure_uart_sensor(UartSensor *sensor, uint8_t new_mode) {
    if (!sensor_ready(sensor)) {
        printf("Input port is not ready!\n");
        return false;
    }

    printf("Setting up uart\n");
    int ret = uart_irq_callback_user_data_set(sensor->dev, serial_cb, sensor);
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
    uart_irq_rx_enable(sensor->dev);
    if (sensor->receive_state == ReceiveState::data &&
        sensor->mode != new_mode) {
        set_sensor_mode(sensor, new_mode);
    }
    sensor->mode = new_mode;
    return true;
}

void uartHeartbeat(UartSensor *sensor) {
    if (sensor->receive_state != ReceiveState::data) {
        return;
    }

    if (!sensor->baudrate_configured) {
        printf("Changing baudrate to %d\n", sensor->baudrate);
        uart_config cfg{};
        uart_config_get(sensor->dev, &cfg);
        cfg.baudrate = sensor->baudrate;

        int config_err = uart_configure(sensor->dev, &cfg);
        printf("config_err = %d\n", config_err);
        if (config_err) {
            printf("UART configure error %d", config_err);
        }

        config_err = uart_config_get(sensor->dev, &cfg);
        printf("current baudrate after config change = %d\n", cfg.baudrate);
        printf("config_err = %d\n", config_err);
        sensor->baudrate_configured = true;

        // Change to the desired mode.
        set_sensor_mode(sensor, sensor->mode);
    }

    uart_poll_out(sensor->dev, 0b00000010);
}

bool sensor_ready(UartSensor *sensor) { return device_is_ready(sensor->dev); }

int get_sensor_value(UartSensor *sensor) {
    if (!sensor->baudrate_configured || sensor->sensor_value < 0) {
        return 0;
    }
    return sensor->sensor_value;
}
#endif