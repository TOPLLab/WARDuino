#ifndef WARDUINO_MOTOR_H
#define WARDUINO_MOTOR_H

#include <stdint.h>
#include <cmath>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

class MotorEncoder {
    static void encoder_pin5_edge_rising(const struct device *dev,
                                         struct gpio_callback *cb,
                                         uint32_t pins) {
        MotorEncoder *encoder =
            CONTAINER_OF(cb, MotorEncoder, pin5_encoder_cb_data);
        if (!encoder->expect_pin5_int) return;

        if (!gpio_pin_get_raw(encoder->pin6_encoder_spec.port,
                              encoder->pin6_encoder_spec.pin)) {
            encoder->angle++;
        } else {
            encoder->angle--;
        }
        encoder->last_update = k_uptime_get();
        // printk("Rising edge detected on encoder pin5, angle %d\n",
        // encoder->angle); printk("%d\n",
        // gpio_pin_get_raw(encoder->pin6_encoder_spec.port,
        // encoder->pin6_encoder_spec.pin));
        encoder->expect_pin5_int = false;
        encoder->expect_pin6_int = true;
    }

    static void encoder_pin6_edge_rising(const struct device *dev,
                                         struct gpio_callback *cb,
                                         uint32_t pins) {
        MotorEncoder *encoder =
            CONTAINER_OF(cb, MotorEncoder, pin6_encoder_cb_data);
        if (!encoder->expect_pin6_int) return;

        if (gpio_pin_get_raw(encoder->pin5_encoder_spec.port,
                             encoder->pin5_encoder_spec.pin)) {
            encoder->angle++;
        } else {
            encoder->angle--;
        }
        // printk("Rising edge detected on encoder pin6, angle %d\n",
        // encoder->angle); printk("%d\n",
        // gpio_pin_get_raw(encoder->pin5_encoder_spec.port,
        // encoder->pin5_encoder_spec.pin));
        encoder->expect_pin6_int = false;
        encoder->expect_pin5_int = true;
    }

   public:
    MotorEncoder(gpio_dt_spec pin5_encoder_spec, gpio_dt_spec pin6_encoder_spec, const char *name);

    ~MotorEncoder();

    inline int get_angle() { return angle; }

    inline void reset_angle() { angle = 0; }

    inline int get_target_angle() { return target_angle; }

    inline void set_target_angle(int new_target_angle) {
        this->target_angle = new_target_angle;
    }

    inline int64_t get_last_update() { return last_update; }

   private:
    gpio_dt_spec pin5_encoder_spec;
    gpio_dt_spec pin6_encoder_spec;
    struct gpio_callback pin5_encoder_cb_data;
    struct gpio_callback pin6_encoder_cb_data;
    volatile int angle;
    int target_angle;
    bool expect_pin5_int;
    bool expect_pin6_int;

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
    void drive_to_angle(int32_t speed, int32_t degrees);
};

bool drive_pwm(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, float pwm1,
               float pwm2);

bool drive_motor(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, float speed);

void drive_motor_to_target(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, MotorEncoder *encoder, int32_t speed);

#endif  // WARDUINO_MOTOR_H
