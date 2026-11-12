#pragma once

#include "drivers/gpio.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

struct gpiod_chip;

// Phase inside one period. duty 0 stays low, duty 1 stays high.
bool pwm_is_high(double seconds, double hz, double duty);

// libgpiod output plus a userspace PWM thread. 1.x and 2.x are both compiled,
// chosen from the pkg-config version (Raspbian 13 ships 2.2).
// shortcut: ~200 us tick, not the Pi's hardware PWM or DMA; jitter under load.
// BCM offsets are line offsets on the chip that owns the 40-pin header.
class PiGpio : public Gpio {
 public:
  explicit PiGpio(const std::string& chip_path);
  ~PiGpio() override;

  void set_output(int pin) override;
  void write(int pin, bool high) override;
  void set_pwm(int pin, double hz, double duty) override;

 private:
  struct Line {
    // gpiod_line* on 1.x, gpiod_line_request* on 2.x.
    void* handle = nullptr;
    bool pwm = false;
    double hz = 0;
    double duty = 0;
    bool level = false;
  };

  void ensure_locked(int pin, int initial);
  void run();

  gpiod_chip* chip_ = nullptr;
  std::unordered_map<int, Line> lines_;
  std::mutex mu_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
};
