#pragma once
#include <cstdint>
#include <zephyr/device.h>

enum ReceiveState {
    advertise,
    final_mode_format,
    modes_complete,
    data
};

struct UartSensor {
    // Variables used for setting up the sensor.
    int payload_bytes = 0;
    int payload_index = 0;
    unsigned int current_payload = 0;
    unsigned char checksum = 0;
    bool data_byte = false;

    // Variables that are used during setup but also by the heartbeat function.
    int baudrate = -1;
    uint8_t mode = 0;
    volatile int sensor_value = 0;
    volatile uint32_t receive_state = ReceiveState::advertise;
    bool baudrate_configured = false;

    // Associated UART device.
    const struct device *dev;

    explicit UartSensor(const struct device *dev) : dev(dev) {}
};

void serial_cb(const struct device *dev, void *user_data);

void uartHeartbeat(UartSensor *sensor);

bool sensor_ready(UartSensor *sensor);

int get_sensor_value(UartSensor *sensor);

bool configure_uart_sensor(UartSensor *sensor, uint8_t new_mode);
