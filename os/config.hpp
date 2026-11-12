#pragma once

#include "services/led.hpp"
#include "services/motion.hpp"

#include <string>

struct Config {
  std::string endpoint = "ipc:///tmp/cocos-api";
  int id = -1;           // cocos_py2/api/coachbot.py starts _id at -1
  int virtual_id = -1;   // same until a config sets it
  // bottom_board_v2.sch TB1, same BCM as cocos src/config.rs. Shared STBY.
  MotorPins left{19, 16, 26, 20};
  MotorPins right{5, 6, 12, 20};
  // SV1 GPIO22/23/24, v1 LED order R/G/B. Schematic does not label colors.
  LedPins led{22, 23, 24};
  // auto: libgpiod when gpiochip exists, otherwise MemoryGpio. memory | pi.
  std::string gpio_backend = "auto";
  std::string gpiochip = "/dev/gpiochip0";
  double motor_hz = 600;  // cocos src/drivers/motor_driver.rs set_speed
  double led_hz = 120;    // cocos src/config.rs LedDescriptor
  int msg_max_bytes = 64;
  int msg_max_count = 32;

  // Flat key=value or key: value. Not a YAML parser.
  bool load_file(const std::string& path, std::string& err);
};

struct CliOverrides {
  bool has_endpoint = false;
  std::string endpoint;
  bool has_id = false;
  int id = -1;
  bool has_virtual_id = false;
  int virtual_id = -1;
};

bool parse_args(int argc, char** argv, Config& cfg, std::string& err);
