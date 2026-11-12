#pragma once

#include "drivers/pi_gpio/pi_gpio_motor.hpp"

// Two-wheel controller. Quadrant map is cocos src/controllers/motor.rs set_vel.
// v1 called left_motor_driver.set_speed for both wheels (motor.rs, the second
// set_speed). That leaves the right PWM pin unchanged. v2 writes the right
// driver for the right wheel.
class MotorController {
 public:
  MotorController(MotorPins left, MotorPins right, double hz)
      : left_(left), right_(right), hz_(hz) {}

  bool ready() const { return left_.pins().configured() && right_.pins().configured(); }

  void init(Gpio& gpio) const {
    left_.init(gpio);
    right_.init(gpio);
  }

  void block(Gpio& gpio) const {
    left_.block(gpio);
    right_.block(gpio);
  }

  // left_pct and right_pct are truncated percents, each in [-100, 100].
  void set_vel(Gpio& gpio, int left_pct, int right_pct) const;

  double hz() const { return hz_; }

 private:
  MotorDriver left_;
  MotorDriver right_;
  double hz_;
};

// cocos src/controllers/api/ipc_requests.rs ApiIpcVelRequestBody: -100..=100.
bool valid_velocity_percent(int percent);
