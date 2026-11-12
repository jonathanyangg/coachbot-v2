#include "api/api_server.h"

#include "api/reply.hpp"

#include <zmq.h>

#include <signal.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

volatile sig_atomic_t g_stop = 0;

void on_signal(int) { g_stop = 1; }

}  // namespace

ApiServer::ApiServer(Config cfg) : cfg_(cfg), dispatcher_(std::move(cfg)) {}

int ApiServer::run() {
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  void* ctx = zmq_ctx_new();
  if (ctx == nullptr) {
    std::cerr << "zmq_ctx_new failed\n";
    return 1;
  }
  void* sock = zmq_socket(ctx, ZMQ_REP);
  if (sock == nullptr) {
    std::cerr << "zmq_socket failed\n";
    zmq_ctx_term(ctx);
    return 1;
  }
  int linger = 0;
  zmq_setsockopt(sock, ZMQ_LINGER, &linger, sizeof(linger));

  if (cfg_.endpoint.rfind("ipc://", 0) == 0) unlink(cfg_.endpoint.c_str() + 6);

  if (zmq_bind(sock, cfg_.endpoint.c_str()) != 0) {
    std::cerr << "zmq_bind " << cfg_.endpoint << " failed: " << zmq_strerror(errno) << "\n";
    zmq_close(sock);
    zmq_ctx_term(ctx);
    return 1;
  }
  std::cout << "cocos2d listening on " << cfg_.endpoint << std::endl;

  const auto started = std::chrono::steady_clock::now();
  auto now_s = [&]() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  };

  while (!g_stop) {
    zmq_pollitem_t item;
    std::memset(&item, 0, sizeof(item));
    item.socket = sock;
    item.events = ZMQ_POLLIN;
    int ready = zmq_poll(&item, 1, 200);
    if (ready < 0) {
      if (errno == EINTR) continue;
      std::cerr << "zmq_poll failed: " << zmq_strerror(errno) << "\n";
      break;
    }
    if (ready == 0) continue;

    zmq_msg_t msg;
    zmq_msg_init(&msg);
    int n = zmq_msg_recv(&msg, sock, 0);
    msgpack::sbuffer reply;
    double t = now_s();
    if (n < 0) {
      reply_err(reply, t, "bad_request");
    } else if (zmq_msg_size(&msg) > (1u << 20)) {
      reply_err(reply, t, "bad_request");
    } else {
      try {
        msgpack::object_handle oh = msgpack::unpack(
            static_cast<const char*>(zmq_msg_data(&msg)), zmq_msg_size(&msg));
        dispatcher_.handle(oh.get(), reply, t);
      } catch (const std::exception&) {
        reply.clear();
        reply_err(reply, t, "bad_request");
      }
    }
    zmq_msg_close(&msg);

    if (zmq_send(sock, reply.data(), reply.size(), 0) < 0) {
      std::cerr << "zmq_send failed: " << zmq_strerror(errno) << "\n";
      break;
    }
  }

  zmq_close(sock);
  zmq_ctx_term(ctx);
  return 0;
}
