#include "config.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {

std::string trim(const std::string& in) {
  size_t begin = 0;
  while (begin < in.size() && std::isspace(static_cast<unsigned char>(in[begin]))) begin++;
  size_t end = in.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(in[end - 1]))) end--;
  return in.substr(begin, end - begin);
}

bool parse_int(const std::string& text, int& out) {
  try {
    size_t used = 0;
    int value = std::stoi(text, &used);
    if (used != text.size()) return false;
    out = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool parse_double(const std::string& text, double& out) {
  try {
    size_t used = 0;
    double value = std::stod(text, &used);
    if (used != text.size()) return false;
    out = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool apply_key(Config& cfg, const std::string& key, const std::string& value, std::string& err) {
  int as_int = 0;
  double as_double = 0;
  auto need_int = [&](int& dest) {
    if (!parse_int(value, as_int)) {
      err = "expected integer for " + key;
      return false;
    }
    dest = as_int;
    return true;
  };

  if (key == "endpoint") {
    if (value.empty()) {
      err = "endpoint is empty";
      return false;
    }
    cfg.endpoint = value;
    return true;
  }
  if (key == "id") return need_int(cfg.id);
  if (key == "virtual_id") return need_int(cfg.virtual_id);
  if (key == "left_in1") return need_int(cfg.left.in1);
  if (key == "left_in2") return need_int(cfg.left.in2);
  if (key == "left_pwm") return need_int(cfg.left.pwm);
  if (key == "left_stdby") return need_int(cfg.left.stdby);
  if (key == "right_in1") return need_int(cfg.right.in1);
  if (key == "right_in2") return need_int(cfg.right.in2);
  if (key == "right_pwm") return need_int(cfg.right.pwm);
  if (key == "right_stdby") return need_int(cfg.right.stdby);
  if (key == "led_r") return need_int(cfg.led.r);
  if (key == "led_g") return need_int(cfg.led.g);
  if (key == "led_b") return need_int(cfg.led.b);
  if (key == "button_pin") return need_int(cfg.button_pin);
  if (key == "gpio") {
    if (value != "auto" && value != "memory" && value != "pi") {
      err = "gpio must be auto, memory, or pi";
      return false;
    }
    cfg.gpio_backend = value;
    return true;
  }
  if (key == "gpiochip") {
    if (value.empty()) {
      err = "gpiochip is empty";
      return false;
    }
    cfg.gpiochip = value;
    return true;
  }
  if (key == "motor_hz") {
    if (!parse_double(value, as_double) || as_double <= 0) {
      err = "motor_hz must be a positive number";
      return false;
    }
    cfg.motor_hz = as_double;
    return true;
  }
  if (key == "led_hz") {
    if (!parse_double(value, as_double) || as_double <= 0) {
      err = "led_hz must be a positive number";
      return false;
    }
    cfg.led_hz = as_double;
    return true;
  }
  if (key == "msg_max_bytes") {
    if (!need_int(cfg.msg_max_bytes) || cfg.msg_max_bytes < 1) {
      err = "msg_max_bytes must be >= 1";
      return false;
    }
    return true;
  }
  if (key == "msg_max_count") {
    if (!need_int(cfg.msg_max_count) || cfg.msg_max_count < 1) {
      err = "msg_max_count must be >= 1";
      return false;
    }
    return true;
  }
  err = "unknown config key: " + key;
  return false;
}

void print_usage() {
  std::cerr
      << "cocos2d [--config FILE] [--endpoint URI] [--id N] [--virtual-id N]\n"
      << "  ZMQ REP at ipc:///tmp/cocos-api (override with --endpoint).\n"
      << "  FILE is flat key=value, not YAML. Default BCM pins are the bottom board.\n"
      << "  gpio=auto|memory|pi. See os/coachbot.example.conf.\n";
}

}  // namespace

bool Config::load_file(const std::string& path, std::string& err) {
  std::ifstream in(path);
  if (!in) {
    err = "could not open " + path;
    return false;
  }
  std::string line;
  int number = 0;
  while (std::getline(in, line)) {
    number++;
    auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;
    auto eq = line.find('=');
    auto colon = line.find(':');
    size_t cut = std::string::npos;
    if (eq != std::string::npos && (colon == std::string::npos || eq < colon)) cut = eq;
    else if (colon != std::string::npos) cut = colon;
    if (cut == std::string::npos) {
      err = path + ":" + std::to_string(number) + ": expected key=value";
      return false;
    }
    std::string key = trim(line.substr(0, cut));
    std::string value = trim(line.substr(cut + 1));
    if (!apply_key(*this, key, value, err)) {
      err = path + ":" + std::to_string(number) + ": " + err;
      return false;
    }
  }
  return true;
}

bool parse_args(int argc, char** argv, Config& cfg, std::string& err) {
  std::string config_path;
  CliOverrides over;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    auto need = [&](std::string& dest) {
      if (i + 1 >= argc) {
        err = "missing value for " + arg;
        return false;
      }
      dest = argv[++i];
      return true;
    };
    if (arg == "--help" || arg == "-h") {
      print_usage();
      err = "help";
      return false;
    }
    if (arg == "--config") {
      if (!need(config_path)) return false;
      continue;
    }
    if (arg == "--endpoint") {
      if (!need(over.endpoint)) return false;
      over.has_endpoint = true;
      continue;
    }
    if (arg == "--id" || arg == "--virtual-id") {
      std::string text;
      if (!need(text)) return false;
      int value = 0;
      if (!parse_int(text, value)) {
        err = arg + " expects an integer";
        return false;
      }
      if (arg == "--id") {
        over.has_id = true;
        over.id = value;
      } else {
        over.has_virtual_id = true;
        over.virtual_id = value;
      }
      continue;
    }
    err = "unknown argument: " + arg;
    print_usage();
    return false;
  }
  if (!config_path.empty() && !cfg.load_file(config_path, err)) return false;
  if (over.has_endpoint) cfg.endpoint = over.endpoint;
  if (over.has_id) cfg.id = over.id;
  if (over.has_virtual_id) cfg.virtual_id = over.virtual_id;
  return true;
}
