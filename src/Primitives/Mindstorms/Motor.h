#ifndef WARDUINO_MOTOR_H
#define WARDUINO_MOTOR_H

#include <cstdint>
#include <cmath>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

class MotorEncoder {
    static void encoder_pin6_edge_rising(const struct device *dev,
                                         struct gpio_callback *cb,
                                         uint32_t pins) {
        MotorEncoder *encoder = CONTAINER_OF(cb, MotorEncoder, pin6_encoder_cb_data);

        int rising = gpio_pin_get_raw(encoder->pin6_encoder_spec.port,
                                      encoder->pin6_encoder_spec.pin);
        int pin5 = gpio_pin_get_raw(encoder->pin5_encoder_spec.port,
                                    encoder->pin5_encoder_spec.pin);

        if (rising) {
            if (pin5) {
                encoder->angle++;
            } else {
                encoder->angle--;
            }
        } else {
            if (pin5) {
                encoder->angle--;
            } else {
                encoder->angle++;
            }
        }
        encoder->last_update = k_uptime_get();
    }

public:
    MotorEncoder(gpio_dt_spec pin5_encoder_spec, gpio_dt_spec pin6_encoder_spec, const char *name);

    ~MotorEncoder();

    [[nodiscard]] inline int get_angle() const { return angle; }

    inline void reset_angle() { angle = 0; }

    [[nodiscard]] inline int get_target_angle() const { return target_angle; }

    inline void set_target_angle(int new_target_angle) {
        this->target_angle = new_target_angle;
    }

    [[nodiscard]] inline int64_t get_last_update() const { return last_update; }

private:
    gpio_dt_spec pin5_encoder_spec;
    gpio_dt_spec pin6_encoder_spec;
    struct gpio_callback pin5_encoder_cb_data;
    struct gpio_callback pin6_encoder_cb_data;
    volatile int angle;
    int target_angle;

public:
    volatile int64_t last_update;
};

class Motor {
public:
    Motor(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, MotorEncoder *encoder);

    pwm_dt_spec pwm1_spec;
    pwm_dt_spec pwm2_spec;
    MotorEncoder *encoder;

    void halt();

    bool set_speed(float speed);

    void drive_to_target(int32_t speed);
    int get_drift();

    void drive_to_angle(int32_t speed, int32_t degrees);
};

bool drive_pwm(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, float pwm1,
               float pwm2);

bool drive_motor(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, float speed);

void drive_motor_to_target(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, MotorEncoder *encoder, int32_t speed);

#endif  // WARDUINO_MOTOR_H
