#include "drivers/esp32/esp32_driver.h"

#include <cstdio>

void esp32_warn_stub() {
  std::fputs(
      "warning: MOTORS_ON_ESP32 is set, but the Feather UART path is not implemented. "
      "setVel returns an error. No frame format is defined. Pi GPIO remains the v1 port.\n",
      stderr);
}

bool esp32_button_available() { return false; }
bool esp32_tof_available() { return false; }
bool esp32_battery_available() { return false; }
