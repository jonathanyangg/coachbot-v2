#pragma once

#include "drivers/gpio.hpp"

struct LedPins {
  int r = -1;
  int g = -1;
  int b = -1;

  bool configured() const { return r >= 0 && g >= 0 && b >= 0; }
};

// cocos src/drivers/led_driver.rs: one PWM duty per channel.
// cocos src/controllers/api/ipc_requests.rs validates LED bytes as 0..=255,
// but the Python client rejects anything outside 0..100
// (cocos_py2/cocos/__init__.py IPCSendLedMessage). v2 follows the client.
bool valid_led_channel(int value);

class LedDriver {
 public:
  LedDriver(LedPins pins, double hz) : pins_(pins), hz_(hz) {}

  bool ready() const { return pins_.configured(); }
  const LedPins& pins() const { return pins_; }

  // Channels are integer percents 0..100. Duty is percent/100.
  // v1 Rust divided the same integers by 255 (messager.rs handle_led_request),
  // so 100 was about 39% duty. v2 treats the published 0..100 range as percent.
  void set_color(Gpio& gpio, int r, int g, int b) const;

 private:
  LedPins pins_;
  double hz_;
};
