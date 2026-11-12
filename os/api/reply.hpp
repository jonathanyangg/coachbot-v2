#pragma once

#include <msgpack.hpp>

#include <cstdint>
#include <string>
#include <vector>

inline void reply_err(msgpack::sbuffer& out, double t, const std::string& err) {
  msgpack::packer<msgpack::sbuffer> pk(&out);
  pk.pack_map(3);
  pk.pack(std::string("ok"));
  pk.pack(false);
  pk.pack(std::string("err"));
  pk.pack(err);
  pk.pack(std::string("t"));
  pk.pack(t);
}

inline void reply_ok_nil(msgpack::sbuffer& out, double t) {
  msgpack::packer<msgpack::sbuffer> pk(&out);
  pk.pack_map(3);
  pk.pack(std::string("ok"));
  pk.pack(true);
  pk.pack(std::string("val"));
  pk.pack_nil();
  pk.pack(std::string("t"));
  pk.pack(t);
}

inline void reply_ok_bool(msgpack::sbuffer& out, double t, bool value) {
  msgpack::packer<msgpack::sbuffer> pk(&out);
  pk.pack_map(3);
  pk.pack(std::string("ok"));
  pk.pack(true);
  pk.pack(std::string("val"));
  pk.pack(value);
  pk.pack(std::string("t"));
  pk.pack(t);
}

inline void reply_ok_int(msgpack::sbuffer& out, double t, int value) {
  msgpack::packer<msgpack::sbuffer> pk(&out);
  pk.pack_map(3);
  pk.pack(std::string("ok"));
  pk.pack(true);
  pk.pack(std::string("val"));
  pk.pack(value);
  pk.pack(std::string("t"));
  pk.pack(t);
}

inline void reply_ok_float(msgpack::sbuffer& out, double t, double value) {
  msgpack::packer<msgpack::sbuffer> pk(&out);
  pk.pack_map(3);
  pk.pack(std::string("ok"));
  pk.pack(true);
  pk.pack(std::string("val"));
  pk.pack(value);
  pk.pack(std::string("t"));
  pk.pack(t);
}

inline void reply_ok_bins(msgpack::sbuffer& out, double t,
                          const std::vector<std::vector<uint8_t>>& msgs) {
  msgpack::packer<msgpack::sbuffer> pk(&out);
  pk.pack_map(3);
  pk.pack(std::string("ok"));
  pk.pack(true);
  pk.pack(std::string("val"));
  pk.pack_array(static_cast<uint32_t>(msgs.size()));
  for (const auto& msg : msgs) {
    pk.pack_bin(msg.size());
    if (!msg.empty()) {
      pk.pack_bin_body(reinterpret_cast<const char*>(msg.data()), msg.size());
    }
  }
  pk.pack(std::string("t"));
  pk.pack(t);
}
