#pragma once

// Feather ESP32 as motor/LED/sensor owner is UNCONFIRMED.
// v1 drove motors and the LED from the Pi (cocos src/config.rs, motor_driver.rs,
// led_driver.rs). These queries stay false until a frame format is specified.
// This header does not define UART bytes.

void esp32_warn_stub();
bool esp32_tof_available();
bool esp32_battery_available();
