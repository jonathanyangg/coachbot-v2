#pragma once

#include "drivers/gpio.hpp"

// Pin roles match cocos src/drivers/motor_driver.rs MotorDescriptor.
// Hardware defaults are the bottom-board TB6612 nets (see Config).
struct MotorPins {
  int in1 = -1;
  int in2 = -1;
  int pwm = -1;
  int stdby = -1;

  bool configured() const { return in1 >= 0 && in2 >= 0 && pwm >= 0 && stdby >= 0; }
};

enum class MotorDirection { Clockwise, CounterClockwise };

// One H-bridge channel. Net pin effect of cocos src/drivers/motor_driver.rs.
class MotorDriver {
 public:
  explicit MotorDriver(MotorPins pins) : pins_(pins) {}

  const MotorPins& pins() const { return pins_; }

  void init(Gpio& gpio) const;
  void block(Gpio& gpio) const;
  void unblock(Gpio& gpio) const;
  void set_direction(Gpio& gpio, MotorDirection direction) const;
  // percent is 0..1. v1 set_speed uses 600 Hz (motor_driver.rs).
  void set_speed(Gpio& gpio, double percent, double hz) const;

 private:
  MotorPins pins_;
};
