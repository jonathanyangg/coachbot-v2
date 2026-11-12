#include "drivers/pi_gpio/pi_gpio_motor.hpp"

void MotorDriver::init(Gpio& gpio) const {
  // cocos src/drivers/motor_driver.rs init: outputs, then all cleared (STBY low).
  gpio.set_output(pins_.in1);
  gpio.set_output(pins_.in2);
  gpio.set_output(pins_.pwm);
  gpio.set_output(pins_.stdby);
  gpio.write(pins_.in1, false);
  gpio.write(pins_.in2, false);
  gpio.write(pins_.pwm, false);
  gpio.write(pins_.stdby, false);
}

void MotorDriver::block(Gpio& gpio) const {
  // IN1=IN2=high and STBY low. cocos src/drivers/motor_driver.rs block.
  gpio.write(pins_.in1, true);
  gpio.write(pins_.in2, true);
  gpio.write(pins_.stdby, false);
}

void MotorDriver::unblock(Gpio& gpio) const {
  // cocos src/drivers/motor_driver.rs unblock.
  gpio.write(pins_.in1, false);
  gpio.write(pins_.in2, false);
  gpio.write(pins_.stdby, true);
}

void MotorDriver::set_direction(Gpio& gpio, MotorDirection direction) const {
  // cocos src/drivers/motor_driver.rs set_direction.
  if (direction == MotorDirection::CounterClockwise) {
    gpio.write(pins_.in1, false);
    gpio.write(pins_.in2, true);
    return;
  }
  gpio.write(pins_.in1, true);
  gpio.write(pins_.in2, false);
}

void MotorDriver::set_speed(Gpio& gpio, double percent, double hz) const {
  gpio.set_pwm(pins_.pwm, hz, percent);
}
