#include "api/dispatcher.hpp"

#include "api/reply.hpp"
#include "drivers/camera/camera_driver.h"
#include "drivers/charger/charger_driver.h"
#include "drivers/esp32/esp32_driver.h"
#include "drivers/pi_gpio/pi_gpio.hpp"
#include "drivers/vive/vive_driver.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {

const msgpack::object* field(const msgpack::object& map, const char* key) {
  if (map.type != msgpack::type::MAP) return nullptr;
  for (uint32_t i = 0; i < map.via.map.size; i++) {
    std::string name;
    try {
      map.via.map.ptr[i].key.convert(name);
    } catch (const std::exception&) {
      continue;
    }
    if (name == key) return &map.via.map.ptr[i].val;
  }
  return nullptr;
}

bool as_number(const msgpack::object* obj, double& out) {
  if (obj == nullptr) return false;
  try {
    obj->convert(out);
  } catch (const std::exception&) {
    return false;
  }
  return std::isfinite(out);
}

bool as_bytes(const msgpack::object* obj, std::vector<uint8_t>& out) {
  if (obj == nullptr) return false;
  const char* ptr = nullptr;
  uint32_t size = 0;
  if (obj->type == msgpack::type::BIN) {
    ptr = obj->via.bin.ptr;
    size = obj->via.bin.size;
  } else if (obj->type == msgpack::type::STR) {
    ptr = obj->via.str.ptr;
    size = obj->via.str.size;
  } else {
    return false;
  }
  if (size == 0 || ptr == nullptr) {
    out.clear();
    return size == 0;
  }
  out.assign(reinterpret_cast<const uint8_t*>(ptr), reinterpret_cast<const uint8_t*>(ptr) + size);
  return true;
}

#ifndef MOTORS_ON_ESP32
bool partial_motor(const MotorPins& pins) {
  int n = (pins.in1 >= 0) + (pins.in2 >= 0) + (pins.pwm >= 0) + (pins.stdby >= 0);
  return n > 0 && n < 4;
}
#endif

std::unique_ptr<Gpio> open_gpio(const Config& cfg) {
  bool chip_exists = access(cfg.gpiochip.c_str(), F_OK) == 0;
  bool use_pi = cfg.gpio_backend == "pi" || (cfg.gpio_backend == "auto" && chip_exists);
  if (!use_pi) {
    std::cerr << "gpio: in-memory backend";
    if (cfg.gpio_backend == "auto") std::cerr << " (" << cfg.gpiochip << " is absent)";
    std::cerr << "\n";
    return std::make_unique<MemoryGpio>();
  }
  std::cerr << "gpio: libgpiod " << cfg.gpiochip << "\n";
  return std::make_unique<PiGpio>(cfg.gpiochip);
}

}  // namespace

Dispatcher::Dispatcher(Config cfg)
    : cfg_(std::move(cfg)),
      gpio_(open_gpio(cfg_)),
      motors_(cfg_.left, cfg_.right, cfg_.motor_hz),
      led_(cfg_.led, cfg_.led_hz),
      inbox_(cfg_.msg_max_bytes, cfg_.msg_max_count) {
#ifdef MOTORS_ON_ESP32
  esp32_warn_stub();
#else
  if (partial_motor(cfg_.left) || partial_motor(cfg_.right)) {
    std::cerr << "warning: motor pin map is incomplete; GPIO will not be driven\n";
  } else if (motors_.ready()) {
    motors_.init(*gpio_);
    // cocos src/controllers/master.rs init blocks the drivers before user code runs.
    motors_.block(*gpio_);
    std::cerr << "motors: left AIN1=" << cfg_.left.in1 << " AIN2=" << cfg_.left.in2
              << " PWMA=" << cfg_.left.pwm << ", right BIN1=" << cfg_.right.in1
              << " BIN2=" << cfg_.right.in2 << " PWMB=" << cfg_.right.pwm
              << ", STBY=" << cfg_.left.stdby << "; held in v1 block (STBY low) until setVel\n";
  } else {
    std::cerr << "motors: pins unset; setVel is accepted and no GPIO is driven\n";
  }
#endif
  if (led_.ready()) {
    std::cerr << "led: BCM r=" << cfg_.led.r << " g=" << cfg_.led.g << " b=" << cfg_.led.b
              << " at " << cfg_.led_hz << " Hz\n";
  } else {
    std::cerr << "led: pins unset; setLed is accepted and no GPIO is driven\n";
  }
}

void Dispatcher::handle(const msgpack::object& request, msgpack::sbuffer& out, double now_s) {
  auto fail = [&](const std::string& err) { reply_err(out, now_s, err); };

  if (request.type != msgpack::type::MAP) {
    fail("bad_request");
    return;
  }
  const msgpack::object* op_obj = field(request, "op");
  std::string op;
  try {
    if (op_obj == nullptr) {
      fail("bad_request");
      return;
    }
    op_obj->convert(op);
  } catch (const std::exception&) {
    fail("bad_request");
    return;
  }

  const msgpack::object* args_obj = field(request, "args");
  if (args_obj != nullptr && !args_obj->is_nil() && args_obj->type != msgpack::type::MAP) {
    fail("bad_request");
    return;
  }
  msgpack::object empty;
  const msgpack::object& args = (args_obj != nullptr && args_obj->type == msgpack::type::MAP)
                                    ? *args_obj
                                    : empty;

  if (op == "setVel") {
#ifdef MOTORS_ON_ESP32
    fail("unavailable: ESP32 motor UART is not implemented");
    return;
#else
    double left_f = 0;
    double right_f = 0;
    if (!as_number(field(args, "left"), left_f) || !as_number(field(args, "right"), right_f)) {
      fail("invalid_args: setVel expects numeric left and right");
      return;
    }
    // cocos_py2/api/coachbot.py truncates with int() before the range check.
    int left = static_cast<int>(std::trunc(left_f));
    int right = static_cast<int>(std::trunc(right_f));
    if (!valid_velocity_percent(left) || !valid_velocity_percent(right)) {
      fail("invalid_args: setVel left and right must be in [-100, 100]");
      return;
    }
    if (motors_.ready()) motors_.set_vel(*gpio_, left, right);
    reply_ok_nil(out, now_s);
    return;
#endif
  }

  if (op == "setLed") {
    double channels[3];
    const char* names[3] = {"r", "g", "b"};
    int rgb[3];
    for (int i = 0; i < 3; i++) {
      if (!as_number(field(args, names[i]), channels[i])) {
        fail("invalid_args: setLed expects numeric r, g, b");
        return;
      }
      double truncated = std::trunc(channels[i]);
      if (std::fabs(channels[i] - truncated) > 1e-6) {
        fail("invalid_args: setLed r, g, b must be integers in [0, 100]");
        return;
      }
      rgb[i] = static_cast<int>(truncated);
      if (!valid_led_channel(rgb[i])) {
        fail("invalid_args: setLed r, g, b must be integers in [0, 100]");
        return;
      }
    }
    if (led_.ready()) led_.set_color(*gpio_, rgb[0], rgb[1], rgb[2]);
    reply_ok_nil(out, now_s);
    return;
  }

  if (op == "getPose") {
    // null matches cocos_py2 get_pose returning None when it has no fix.
    // A true result from the stub still has no frame parser, so it is an error
    // rather than a made-up pose. nucifera_driver.rs defines no packet bytes.
    if (vive_pose_available()) {
      fail("unavailable: vive pose has no frame parser");
      return;
    }
    reply_ok_nil(out, now_s);
    return;
  }

  if (op == "getPoseCamera") {
    if (!camera_pose_available()) {
      fail("unavailable: camera pose is not implemented");
      return;
    }
    fail("unavailable: camera pose has no frame parser");
    return;
  }

  if (op == "sendMsg") {
    std::vector<uint8_t> msg;
    if (!as_bytes(field(args, "msg"), msg)) {
      fail("invalid_args: sendMsg expects msg bytes");
      return;
    }
    inbox_.send(std::move(msg));
    reply_ok_nil(out, now_s);
    return;
  }

  if (op == "recvMsg") {
    reply_ok_bins(out, now_s, inbox_.recv());
    return;
  }

  if (op == "getClock") {
    // Monotonic seconds since this process started. Not a swarm-wide sync.
    reply_ok_float(out, now_s, now_s);
    return;
  }

  if (op == "delay") {
    // v1 sleeps in the client (cocos_py2/api/coachbot.py). The handler returns
    // immediately so the REP socket is not held for the wait.
    double ms_f = 0;
    if (!as_number(field(args, "ms"), ms_f)) {
      fail("invalid_args: delay expects numeric ms");
      return;
    }
    if (static_cast<int>(std::trunc(ms_f)) < 0) {
      fail("invalid_args: delay ms must be >= 0");
      return;
    }
    reply_ok_nil(out, now_s);
    return;
  }

  if (op == "getId") {
    reply_ok_int(out, now_s, cfg_.id);
    return;
  }

  if (op == "getVirtualId") {
    reply_ok_int(out, now_s, cfg_.virtual_id);
    return;
  }

  if (op == "sendStartTime") {
    double when = 0;
    if (!as_number(field(args, "t"), when)) {
      fail("invalid_args: sendStartTime expects numeric t");
      return;
    }
    start_time_ = when;
    has_start_ = true;
    std::cerr << "sendStartTime " << start_time_ << "\n";
    (void)has_start_;
    reply_ok_nil(out, now_s);
    return;
  }

  if (op == "isButtonPressed") {
    if (!esp32_button_available()) {
      fail("unavailable: button is not wired");
      return;
    }
    fail("unavailable: button sample is not implemented");
    return;
  }

  if (op == "isCharging") {
    if (!charger_sense_available()) {
      fail("unavailable: charger sense is not wired");
      return;
    }
    fail("unavailable: charger sample is not implemented");
    return;
  }

  if (op == "getDistanceTOF") {
    if (!esp32_tof_available()) {
      fail("unavailable: time-of-flight sensor is not wired");
      return;
    }
    fail("unavailable: time-of-flight sample is not implemented");
    return;
  }

  if (op == "getBatteryVoltage") {
    if (!esp32_battery_available()) {
      fail("unavailable: battery sense is not wired");
      return;
    }
    fail("unavailable: battery sample is not implemented");
    return;
  }

  // Snake_case v1 names land here on purpose. No aliases.
  fail("unknown_op");
}
