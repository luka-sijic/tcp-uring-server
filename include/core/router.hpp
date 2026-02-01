#pragma once
#include <iostream>

#include "core/parser.hpp"
#include "models/net.h"
#include "net/net_out.hpp"
#include <unordered_map>

// enum class Op : uint8_t { Register = 0, Disconnect = 1, Update = 2 };

class Router {
public:
  Router(INetOut &out) : out_(out) {}

  void on_packet(const PacketView &pkt) {
    auto decoded = parser_.parse(pkt.bytes);
    std::cerr << decoded.op << " " << decoded.id << " " << decoded.x << " "
              << decoded.y << '\n';
    switch (decoded.op) {
    case 0:
      on_register(pkt, decoded);
      break;
    case 1:
      on_disconnect(pkt, decoded);
      break;
    case 2:
      on_update(pkt, decoded);
      break;
    default:
      std::cerr << "Unknown OP recv" << '\n';
      break;
    }
  }

private:
  void on_register(const PacketView &pkt, const Players &p) {
    players_[p.id] = {pkt.peer, pkt.peer_len};
    std::cerr << "Player added: " << p.id << " " << p.op << '\n';
    broadcast_all(&p, sizeof(Players));
  }

  void on_disconnect(const PacketView &pkt, const Players &p) {
    players_.erase(p.id);
    std::cerr << "Player disconnected: " << p.id << '\n';
  }

  void broadcast_all(const void *data, size_t len) {
    for (auto const &[id, peer] : players_) {
      out_.send_to(peer.addr, peer.len, data, len);
    }
  }

  void broadcast_all_except(const void *data, size_t len, int pid) {
    for (auto const &[id, peer] : players_) {
      if (id != pid) {
        out_.send_to(peer.addr, peer.len, data, len);
      }
    }
  }

  void on_update(const PacketView &pkt, const Players &p) {
    std::cerr << "Sending data..." << '\n';
    broadcast_all(&p, sizeof(Players));
  }

  Parser parser_;
  INetOut &out_;
  std::unordered_map<uint32_t, PeerInfo> players_;
};
