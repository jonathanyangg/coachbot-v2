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
};

struct GpioPin {
  bool driven = false;
  bool level = false;
  bool pwm = false;
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

  const GpioPin* get(int pin) const {
    auto it = pins_.find(pin);
    if (it == pins_.end()) return nullptr;
    return &it->second;
  }

 private:
  std::map<int, GpioPin> pins_;
};
