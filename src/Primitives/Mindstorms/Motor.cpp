#ifndef ARDUINO
#include "Motor.h"

#include "../../Utils/macros.h"

MotorEncoder::MotorEncoder(gpio_dt_spec pin5_encoder_spec,
                           gpio_dt_spec pin6_encoder_spec, const char *name)
    : pin5_encoder_spec(pin5_encoder_spec),
      pin6_encoder_spec(pin6_encoder_spec),
      angle(0),
      target_angle(0),
      last_update(0), speed(0.0f), ticks(0) {
    if (gpio_pin_configure_dt(&pin5_encoder_spec, GPIO_INPUT)) {
        FATAL("Failed to configure GPIO encoder pin5\n");
    }
    if (gpio_pin_configure_dt(&pin6_encoder_spec, GPIO_INPUT)) {
        FATAL("Failed to configure GPIO encoder pin6\n");
    }

    int result = gpio_pin_interrupt_configure_dt(
        &pin6_encoder_spec, GPIO_INT_EDGE_RISING | GPIO_INT_EDGE_FALLING);
    if (result != 0) {
        printf("Failed to configure interrupt on pin6 for %s, error code %d\n",
               name, result);
    }
    gpio_init_callback(&pin6_encoder_cb_data,
                       MotorEncoder::encoder_pin6_edge_rising,
                       BIT(pin6_encoder_spec.pin));
    gpio_add_callback(pin6_encoder_spec.port, &pin6_encoder_cb_data);
}

MotorEncoder::~MotorEncoder() {
    gpio_remove_callback(pin5_encoder_spec.port, &pin5_encoder_cb_data);
    gpio_remove_callback(pin6_encoder_spec.port, &pin6_encoder_cb_data);
}

bool drive_pwm(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec, float pwm1,
               float pwm2) {
    if (!pwm_is_ready_dt(&pwm1_spec)) {
        printf("Error: PWM device %s is not ready\n", pwm1_spec.dev->name);
        return false;
    }

    int ret = pwm_set_pulse_dt(&pwm1_spec, pwm1 * pwm1_spec.period);
    if (ret) {
        printf("Error %d: failed to set pulse width, pwm1 = %f\n", ret, pwm1);
        return false;
    }

    ret = pwm_set_pulse_dt(&pwm2_spec, pwm2 * pwm2_spec.period);
    if (ret) {
        printf("Error %d: failed to set pulse width, pwm2 = %f\n", ret, pwm2);
        return false;
    }
    return true;
}

Motor::Motor(pwm_dt_spec pwm1_spec, pwm_dt_spec pwm2_spec,
             MotorEncoder *encoder)
    : pwm1_spec(pwm1_spec), pwm2_spec(pwm2_spec), encoder(encoder) {}

void Motor::halt() { drive_pwm(pwm1_spec, pwm2_spec, 1.0f, 1.0f); }

bool Motor::set_speed(float speed) {
    float pwm1 = 0;
    float pwm2 = 0;
    if (speed > 0) {
        pwm1 = speed;
    } else {
        pwm2 = -speed;
    }

    return drive_pwm(pwm1_spec, pwm2_spec, pwm1, pwm2);
}

float Kp = 1.0f;
float Kd = 0.2f;
float Ki = 0.05f;
//float Ki = 0.01f;
//float Kd = 0.2f;

float clamp(float v, float min, float max) {
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

struct PID {
    float Kp;
    float Kd;
    float Ki;

    float integral;
    float prev_error;
    float dt;

    PID(const float Kp, const float Ki, const float Kd) :
        Kp(Kp), Kd(Kd), Ki(Ki), integral(0.0f), prev_error(0.0f), dt(0.01f)  {}

    float update(const float error) {
        integral += error * dt;
        integral = clamp(integral, -20000.0f, 20000.0f);

        const float derivative = (error - prev_error) / dt;

        const float output =
            Kp * error + // Proportional
            Ki * integral + // Integral
            Kd * derivative; // Derivative

        prev_error = error;
        return output;
    }
};

void Motor::drive_to_target(int32_t _s) {
    printf("drift = %d\n", abs(get_drift()));

    // ev3 has rpm of 160-170
    // so maximum around a 1000 degrees per second
    float max_speed = 350.0f; // Degrees per second
    float target_speed = max_speed; // Degrees per second

    PID speed_pid(1.0f, 0.05f, 0.2f);
    PID position_pid(2.5f, 0.002f, 0.09f);

    while (true) {
        // Control speed.
        float encoder_speed = encoder->get_speed();
        float error = target_speed - encoder_speed;

        float output = speed_pid.update(error);

        float speed = clamp(output, -10000.0f, 10000.0f);
        float normalized_speed = speed / 10000.0f;
        set_speed(normalized_speed);

        // Control position.
        error = static_cast<float>(get_drift());
        output = position_pid.update(error);
        target_speed = clamp(output, -max_speed, max_speed);

        printf("position error = %f\n", error);

        //printf("error = %f, speed = %f, integral = %f, derivative = %f, encoder_speed = %f\n", error, speed, integral, derivative, encoder_speed);

        if (error == 0.0f && position_pid.prev_error == 0.0f) {
            break;
        }
    }
    halt();

    /*int drift = get_drift();
    // Reset stall timer, otherwise it will instantly think it's not moving.
    encoder->last_update = k_uptime_get();
    while (abs(drift) > 0) {
        int speed_sign = std::signbit(drift) ? -1 : 1;
        set_speed(speed_sign * speed / 10000.0f);
        while (speed_sign * (get_drift()) > 0 &&
               k_uptime_get() - encoder->get_last_update() < 150) {
        }
        bool not_moving = k_uptime_get() - encoder->get_last_update() >= 150;
        if (not_moving) {
            speed += 100;
            printf(
                "Not moving, increasing speed to %d, %llims since last "
                "movement\n",
                speed, k_uptime_get() - encoder->get_last_update());
            set_speed(speed_sign * speed / 10000.0f);

            // Wait for 10ms or movement.
            uint64_t start_time = k_uptime_get();
            while (k_uptime_get() - start_time < 10 &&
                   k_uptime_get() - encoder->get_last_update() >= 150) {
            }
            continue;
        }
        encoder->last_update = k_uptime_get();
        printf("elapsed = %lli\n", k_uptime_get() - encoder->get_last_update());
        halt();
        k_msleep(50);
        drift = get_drift();
        printf("drift = %d, speed = %d\n", drift, speed);
        speed = 800;
    }*/
}

int Motor::get_drift() {
    return encoder->get_target_angle() - encoder->get_angle();
}

void Motor::drive_to_angle(int32_t speed, int32_t degrees) {
    printf("Drive to angle %d\n", degrees);
    encoder->set_target_angle(degrees);
    drive_to_target(speed);
}

#endif
