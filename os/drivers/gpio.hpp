#pragma once

#include <map>

// Same calls as v1 DrivesGpio + DrivesPwm (cocos src/io/interface/gpio.rs, pwm.rs).
class Gpio {
 public:
  virtual ~Gpio() = default;
  virtual void set_output(int pin) = 0;
  virtual void write(int pin, bool high) = 0;
  // duty is 0..1, matching rppal set_pwm_frequency in cocos src/io/rpi/pwm.rs.
  virtual void set_pwm(int pin, double hz, double duty) = 0;
  // Input with the internal pull-down on (active-high button to 3.3V).
  virtual void set_input_pull_down(int pin) = 0;
  virtual bool read(int pin) = 0;
};

struct GpioPin {
  bool driven = false;
  bool level = false;
  bool pwm = false;
  bool input = false;
  bool pull_down = false;
  double hz = 0;
  double duty = 0;
};

// Test double. Does not touch SoC registers.
class MemoryGpio : public Gpio {
 public:
  void set_output(int pin) override { pins_[pin].driven = true; }

  void write(int pin, bool high) override {
    GpioPin& p = pins_[pin];
    p.driven = true;
    p.level = high;
    p.pwm = false;
  }

  void set_pwm(int pin, double hz, double duty) override {
    GpioPin& p = pins_[pin];
    p.driven = true;
    p.pwm = true;
    p.hz = hz;
    p.duty = duty;
  }

  void set_input_pull_down(int pin) override {
    GpioPin& p = pins_[pin];
    p.input = true;
    p.pull_down = true;
    p.driven = false;
    p.pwm = false;
  }

  // An unconnected input reads low because of the pull-down.
  bool read(int pin) override {
    auto it = pins_.find(pin);
    return it != pins_.end() && it->second.level;
  }

  // Tests only: drive the level an input will read.
  void inject(int pin, bool high) { pins_[pin].level = high; }

  const GpioPin* get(int pin) const {
    auto it = pins_.find(pin);
    if (it == pins_.end()) return nullptr;
    return &it->second;
  }

 private:
  std::map<int, GpioPin> pins_;
};
