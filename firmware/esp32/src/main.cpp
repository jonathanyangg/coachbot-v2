#include <Arduino.h>

// UNCONFIRMED. v1 cocos drove motors and the LED from the Pi
// (src/config.rs, src/drivers/motor_driver.rs, src/drivers/led_driver.rs).
// This sketch does not own those pins and does not define a UART frame.
// Do not invent CRC, heartbeat, or motor packets here.
void setup() { Serial.begin(115200); }
void loop() {}
