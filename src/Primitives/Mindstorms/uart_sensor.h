#pragma once
#include <cstdint>

enum ReceiveState {
    advertise,
    final_mode_format,
    modes_complete,
    data
};

void serial_cb([[maybe_unused]] const struct device *dev, [[maybe_unused]] void *user_data);

void set_sensor_mode(uint8_t new_mode);

void uartHeartbeat();

bool sensor_ready();

int get_sensor_value();

bool configure_uart_sensor(uint8_t new_mode);
