#include "config.hpp"
#include "drivers/pi_gpio/pi_gpio.hpp"
#include "services/comms.hpp"
#include "services/led.hpp"
#include "services/motion.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool cond, const char* message) {
  if (cond) return;
  std::cerr << "FAIL " << message << "\n";
  g_fails++;
}

bool level(const MemoryGpio& gpio, int pin) {
  const GpioPin* p = gpio.get(pin);
  return p != nullptr && !p->pwm && p->level;
}

double duty(const MemoryGpio& gpio, int pin) {
  const GpioPin* p = gpio.get(pin);
  if (p == nullptr || !p->pwm) return -1;
  return p->duty;
}

double hz(const MemoryGpio& gpio, int pin) {
  const GpioPin* p = gpio.get(pin);
  if (p == nullptr || !p->pwm) return -1;
  return p->hz;
}

}  // namespace

int main() {
  Config defaults;
  expect(defaults.left.in1 == 19 && defaults.left.in2 == 16 && defaults.left.pwm == 26 &&
             defaults.left.stdby == 20,
         "default left BCM");
  expect(defaults.right.in1 == 5 && defaults.right.in2 == 6 && defaults.right.pwm == 12 &&
             defaults.right.stdby == 20,
         "default right BCM");
  expect(defaults.led.r == 22 && defaults.led.g == 23 && defaults.led.b == 24, "default LED BCM");
  expect(defaults.button_pin == 27, "default button BCM");
  expect(!pwm_is_high(0.0, 600, 0.0) && pwm_is_high(0.0, 600, 1.0), "pwm endpoints");
  expect(pwm_is_high(0.0001, 600, 0.5) && !pwm_is_high(0.001, 600, 0.5), "pwm half period");
  {
    Config file;
    std::string err;
    expect(file.load_file(EXAMPLE_CONF, err), "example conf loads");
    expect(file.left.in1 == 19 && file.left.pwm == 26 && file.right.in1 == 5 && file.right.pwm == 12 &&
               file.left.stdby == 20 && file.right.stdby == 20 && file.led.r == 22 && file.led.b == 24,
           "example conf pins");
    expect(file.button_pin == 27, "example conf button");
  }
  {
    MemoryGpio buttons;
    buttons.set_input_pull_down(27);
    const GpioPin* pin = buttons.get(27);
    expect(pin != nullptr && pin->input && pin->pull_down && !pin->driven, "button input pull-down");
    expect(!buttons.read(27), "button released reads low");
    buttons.inject(27, true);
    expect(buttons.read(27), "button pressed reads high");
  }

  // Same BCM numbers, injected into the in-memory backend.
  MotorPins left{19, 16, 26, 20};
  MotorPins right{5, 6, 12, 20};
  MemoryGpio gpio;
  MotorController motors(left, right, 600);
  motors.init(gpio);
  motors.block(gpio);
  expect(level(gpio, 19) && level(gpio, 16), "block left inputs high");
  expect(level(gpio, 5) && level(gpio, 6), "block right inputs high");
  expect(!level(gpio, 20), "block shared STBY low");

  motors.set_vel(gpio, 100, 100);
  expect(!level(gpio, 19) && level(gpio, 16), "forward left is CCW");
  expect(level(gpio, 5) && !level(gpio, 6), "forward right is CW");
  expect(level(gpio, 20), "running STBY high");
  expect(std::fabs(duty(gpio, 26) - 1.0) < 1e-9, "left duty 1");
  expect(std::fabs(duty(gpio, 12) - 1.0) < 1e-9, "right duty 1");
  expect(std::fabs(hz(gpio, 26) - 600) < 1e-9, "motor pwm 600 Hz");

  motors.set_vel(gpio, -100, 50);
  expect(level(gpio, 19) && !level(gpio, 16), "negative left is CW");
  expect(level(gpio, 5) && !level(gpio, 6), "positive right stays CW");
  expect(std::fabs(duty(gpio, 26) - 1.0) < 1e-9, "left abs duty");
  expect(std::fabs(duty(gpio, 12) - 0.5) < 1e-9, "right half duty");

  motors.set_vel(gpio, 0, -100);
  expect(!level(gpio, 19) && level(gpio, 16), "zero left is the non-negative quadrant");
  expect(!level(gpio, 5) && level(gpio, 6), "negative right is CCW");
  expect(std::fabs(duty(gpio, 26) - 0.0) < 1e-9, "zero left duty");
  expect(std::fabs(duty(gpio, 12) - 1.0) < 1e-9, "full right duty on the right pin");

  expect(valid_velocity_percent(-100) && valid_velocity_percent(100) && valid_velocity_percent(0),
         "velocity endpoints");
  expect(!valid_velocity_percent(101) && !valid_velocity_percent(-101), "velocity outside");

  LedPins led_pins{22, 23, 24};
  LedDriver led(led_pins, 120);
  led.set_color(gpio, 0, 100, 50);
  expect(std::fabs(duty(gpio, 22) - 0.0) < 1e-9, "led red off");
  expect(std::fabs(duty(gpio, 23) - 1.0) < 1e-9, "led green full");
  expect(std::fabs(duty(gpio, 24) - 0.5) < 1e-9, "led blue half");
  expect(std::fabs(hz(gpio, 22) - 120) < 1e-9, "led 120 Hz");
  expect(valid_led_channel(0) && valid_led_channel(100) && !valid_led_channel(101) &&
             !valid_led_channel(-1),
         "led range");

  Inbox limited(64, 2);
  limited.send(std::vector<uint8_t>(80, 7));
  auto first = limited.recv();
  expect(first.size() == 1 && first[0].size() == 64 && first[0][0] == 7, "truncate to 64 bytes");
  expect(limited.recv().empty(), "recv drains the inbox");

  Inbox capped(64, 2);
  capped.send(std::vector<uint8_t>{1});
  capped.send(std::vector<uint8_t>{2});
  capped.send(std::vector<uint8_t>{3});
  auto kept = capped.recv();
  expect(kept.size() == 2 && kept[0].size() == 1 && kept[0][0] == 2 && kept[1][0] == 3,
         "drop the oldest past max_count");

  if (g_fails != 0) {
    std::cerr << g_fails << " failed\n";
    return 1;
  }
  std::cout << "logic ok\n";
  return 0;
}
