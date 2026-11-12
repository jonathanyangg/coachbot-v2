#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// In-process inbox. There is no radio in this build.
// v1 cocos_py2/api/coachbot.py says messages longer than
// coach_os.custom_net.MSG_LEN-8 are trimmed, but that constant is not in the
// public cocos repo. The published user guide truncates at 64 bytes
// (https://coachbotswarm.github.io/User-Guide/). 64 is that documented cap,
// not a radio frame layout.
// shortcut: MAX_MSG_NUM is named in cocos_py2/api/coachbot.py and not given a
// value; max_count is only a local bound so the inbox cannot grow forever.
class Inbox {
 public:
  Inbox(int max_bytes, int max_count) : max_bytes_(max_bytes), max_count_(max_count) {}

  void send(std::vector<uint8_t> msg) {
    if (static_cast<int>(msg.size()) > max_bytes_) msg.resize(static_cast<size_t>(max_bytes_));
    box_.push_back(std::move(msg));
    while (static_cast<int>(box_.size()) > max_count_) box_.pop_front();
  }

  // v1 recv_msg returns what arrived since the last call (coachbot.py).
  // v2 proto val is a list, so an empty inbox is [] rather than the user-guide None.
  std::vector<std::vector<uint8_t>> recv() {
    std::vector<std::vector<uint8_t>> out(box_.begin(), box_.end());
    box_.clear();
    return out;
  }

 private:
  int max_bytes_;
  int max_count_;
  std::deque<std::vector<uint8_t>> box_;
};
