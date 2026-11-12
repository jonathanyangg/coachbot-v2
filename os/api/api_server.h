#pragma once

#include "api/dispatcher.hpp"
#include "config.hpp"

// ZMQ REP loop. One msgpack request in, one reply out, at cfg.endpoint.
class ApiServer {
 public:
  explicit ApiServer(Config cfg);
  int run();

 private:
  Config cfg_;
  Dispatcher dispatcher_;
};
