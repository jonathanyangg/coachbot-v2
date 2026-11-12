#pragma once

// v1 cocos src/drivers/nucifera_driver.rs read_current_position is unimplemented
// and returns Position::zero(). The file defines UART line settings in config.rs
// (19200 8E1) and no packet bytes. v2 does not invent a Vive or nucifera frame.
bool vive_pose_available();
