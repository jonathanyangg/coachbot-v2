#include "services/motion.hpp"

#include <cstdlib>

bool valid_velocity_percent(int percent) { return percent >= -100 && percent <= 100; }

namespace {

MotorDirection direction_for(bool clockwise) {
  return clockwise ? MotorDirection::Clockwise : MotorDirection::CounterClockwise;
}

}  // namespace

void MotorController::set_vel(Gpio& gpio, int left_pct, int right_pct) const {
  // v1 MotorPower::is_locked is not an IPC field (messager always passes false).
  // Startup braking is MotorController::block, same as master.rs init.
  left_.unblock(gpio);
  right_.unblock(gpio);

  // Quadrants from cocos src/controllers/motor.rs. Positive left is CCW and
  // positive right is CW so a mirrored pair rolls forward.
  bool left_cw = false;
  bool right_cw = true;
  if (left_pct >= 0 && right_pct >= 0) {
    left_cw = false;
    right_cw = true;
  } else if (left_pct >= 0 && right_pct < 0) {
    left_cw = false;
    right_cw = false;
  } else if (left_pct < 0 && right_pct >= 0) {
    left_cw = true;
    right_cw = true;
  } else {
    left_cw = true;
    right_cw = false;
  }

  left_.set_direction(gpio, direction_for(left_cw));
  right_.set_direction(gpio, direction_for(right_cw));

  // cocos src/controllers/api/messager.rs handle_vel_request divides by 100,
  // then MotorDriver::set_speed takes a 0..1 duty.
  left_.set_speed(gpio, std::abs(left_pct) / 100.0, hz_);
  right_.set_speed(gpio, std::abs(right_pct) / 100.0, hz_);
}
