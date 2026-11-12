#include "api/api_server.h"
#include "config.hpp"

#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
  Config cfg;
  std::string err;
  if (!parse_args(argc, argv, cfg, err)) {
    if (err == "help") return 0;
    std::cerr << err << "\n";
    return 2;
  }
  std::cerr << "cocos2d id=" << cfg.id << " virtual_id=" << cfg.virtual_id << "\n";
  try {
    ApiServer server(std::move(cfg));
    return server.run();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}
