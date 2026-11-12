#pragma once

#include "config.hpp"
#include "services/comms.hpp"

#include <memory>
#include <msgpack.hpp>

// Dispatches one msgpack request into a reply buffer. Handlers do not sleep.
class Dispatcher {
 public:
  explicit Dispatcher(Config cfg);
  void handle(const msgpack::object& request, msgpack::sbuffer& out, double now_s);

 private:
  Config cfg_;
  std::unique_ptr<Gpio> gpio_;
  MotorController motors_;
  LedDriver led_;
  Inbox inbox_;
  bool button_ready_ = false;
  double start_time_ = 0;
  bool has_start_ = false;
};
