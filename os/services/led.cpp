#include "services/led.hpp"

bool valid_led_channel(int value) { return value >= 0 && value <= 100; }

void LedDriver::set_color(Gpio& gpio, int r, int g, int b) const {
  // Default 120 Hz is cocos src/config.rs LedDescriptor.
  gpio.set_pwm(pins_.r, hz_, r / 100.0);
  gpio.set_pwm(pins_.g, hz_, g / 100.0);
  gpio.set_pwm(pins_.b, hz_, b / 100.0);
}
