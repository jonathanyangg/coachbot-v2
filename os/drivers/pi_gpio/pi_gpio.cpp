#include "drivers/pi_gpio/pi_gpio.hpp"

#include <gpiod.h>

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

bool pwm_is_high(double seconds, double hz, double duty) {
  if (!(hz > 0.0) || duty <= 0.0) return false;
  if (duty >= 1.0) return true;
  double period = 1.0 / hz;
  double phase = std::fmod(seconds, period);
  if (phase < 0.0) phase += period;
  return phase < duty * period;
}

namespace {

int apply_level(void* handle, int pin, bool high) {
#ifdef COCOS_GPIOD_V2
  // libgpiod 2.2: gpiod_line_request_set_value(request, offset, value).
  auto* request = static_cast<gpiod_line_request*>(handle);
  enum gpiod_line_value value = high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
  return gpiod_line_request_set_value(request, static_cast<unsigned>(pin), value);
#else
  (void)pin;
  return gpiod_line_set_value(static_cast<gpiod_line*>(handle), high ? 1 : 0);
#endif
}

void release_handle(void* handle) {
  if (handle == nullptr) return;
#ifdef COCOS_GPIOD_V2
  gpiod_line_request_release(static_cast<gpiod_line_request*>(handle));
#else
  auto* line = static_cast<gpiod_line*>(handle);
  if (gpiod_line_is_requested(line)) gpiod_line_release(line);
#endif
}

}  // namespace

PiGpio::PiGpio(const std::string& chip_path) {
  chip_ = gpiod_chip_open(chip_path.c_str());
  if (chip_ == nullptr) {
    throw std::runtime_error("gpiod_chip_open " + chip_path + " failed: " + std::strerror(errno) +
                             " (need the gpio group or root, and the header's gpiochip)");
  }
  thread_ = std::thread([this] { run(); });
}

PiGpio::~PiGpio() {
  stop_.store(true);
  if (thread_.joinable()) thread_.join();
  for (auto& entry : lines_) release_handle(entry.second.handle);
  if (chip_ != nullptr) gpiod_chip_close(chip_);
}

void PiGpio::ensure_locked(int pin, int initial) {
  Line& state = lines_[pin];
  if (state.handle != nullptr) return;
#ifdef COCOS_GPIOD_V2
  // libgpiod 2.x has no gpiod_chip_get_line / gpiod_line_request_output.
  // Each BCM offset gets its own line request. A second call for the same
  // pin (shared STBY) hits the handle check above.
  unsigned int offset = static_cast<unsigned>(pin);
  gpiod_line_settings* settings = gpiod_line_settings_new();
  gpiod_line_config* line_cfg = gpiod_line_config_new();
  gpiod_request_config* req_cfg = gpiod_request_config_new();
  if (settings == nullptr || line_cfg == nullptr || req_cfg == nullptr) {
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    throw std::runtime_error("libgpiod alloc failed");
  }
  enum gpiod_line_value value =
      initial ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
  int ready = gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
  if (ready == 0) ready = gpiod_line_settings_set_output_value(settings, value);
  if (ready == 0) ready = gpiod_line_config_add_line_settings(line_cfg, &offset, 1, settings);
  gpiod_request_config_set_consumer(req_cfg, "cocos2d");
  gpiod_line_request* request = nullptr;
  if (ready == 0) request = gpiod_chip_request_lines(chip_, req_cfg, line_cfg);
  int saved = errno;
  gpiod_request_config_free(req_cfg);
  gpiod_line_config_free(line_cfg);
  gpiod_line_settings_free(settings);
  if (request == nullptr) {
    throw std::runtime_error("gpiod_chip_request_lines BCM " + std::to_string(pin) +
                             " failed: " + std::strerror(saved));
  }
  state.handle = request;
#else
  auto* line = gpiod_chip_get_line(chip_, static_cast<unsigned>(pin));
  if (line == nullptr) {
    throw std::runtime_error("gpiod_chip_get_line BCM " + std::to_string(pin) + " failed");
  }
  if (gpiod_line_request_output(line, "cocos2d", initial) < 0) {
    throw std::runtime_error("gpiod_line_request_output BCM " + std::to_string(pin) +
                             " failed: " + std::strerror(errno));
  }
  state.handle = line;
#endif
  state.level = initial != 0;
}

void PiGpio::set_output(int pin) {
  std::lock_guard<std::mutex> lock(mu_);
  ensure_locked(pin, 0);
}

void PiGpio::write(int pin, bool high) {
  std::lock_guard<std::mutex> lock(mu_);
  ensure_locked(pin, high ? 1 : 0);
  Line& state = lines_[pin];
  state.pwm = false;
  state.level = high;
  if (apply_level(state.handle, pin, high) < 0) {
    throw std::runtime_error("gpio set BCM " + std::to_string(pin) + " failed");
  }
}

void PiGpio::set_pwm(int pin, double hz, double duty) {
  std::lock_guard<std::mutex> lock(mu_);
  ensure_locked(pin, 0);
  Line& state = lines_[pin];
  state.pwm = true;
  state.hz = hz;
  state.duty = duty;
}

void PiGpio::run() {
  const auto started = std::chrono::steady_clock::now();
  while (!stop_.load()) {
    double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (auto& entry : lines_) {
        Line& state = entry.second;
        if (!state.pwm || state.handle == nullptr) continue;
        bool high = pwm_is_high(seconds, state.hz, state.duty);
        if (high == state.level) continue;
        if (apply_level(state.handle, entry.first, high) == 0) state.level = high;
      }
    }
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }
}
