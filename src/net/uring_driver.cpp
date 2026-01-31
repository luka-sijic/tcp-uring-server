#include "net/uring_driver.hpp"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <signal.h>

#include "core/parser.hpp"
#include "models/response.h"

static void dump_hex(const char *p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    unsigned v = static_cast<unsigned>(static_cast<unsigned char>(p[i]));
    std::cout << std::hex << std::setw(2) << std::setfill('0') << v << ' ';
  }
  std::cout << std::dec << '\n';
}

static constexpr unsigned kQueueDepth = 1024;
static constexpr size_t kAcceptPipeline = 256;

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

UringDriver::UringDriver(Router *r, int fd) : fd_(fd) {
  signal(SIGINT, on_sigint);

  router_ = *r;
  if (fd_ < 0)
    throw ::std::runtime_error("failed to create listen socket");

  if (io_uring_queue_init(kQueueDepth, &ring_, 0) < 0) {
    ::close(fd_);
    throw ::std::runtime_error("io_uring_queue_init failed");
  }

  for (int i = 0; i < kUdpSlots; ++i) {
    auto &s = udp_[i];

    s.iov.iov_base = s.buf;
    s.iov.iov_len = UdpState::kBufSize;

    std::memset(&s.msg, 0, sizeof(s.msg));
    s.peer_len = sizeof(s.peer);
    s.msg.msg_name = &s.peer;
    s.msg.msg_namelen = s.peer_len;
    s.msg.msg_iov = &s.iov;
    s.msg.msg_iovlen = 1;
  }

  submit_recv(0);
  submit_recv(1);
  io_uring_submit(&ring_);
}

bool UringDriver::submit_recv(uint32_t slot) {
  auto &s = udp_[slot];

  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    s.peer_len = sizeof(s.peer);
    s.msg.msg_namelen = s.peer_len;
    s.iov.iov_base = s.buf;
    s.iov.iov_len = UdpState::kBufSize;

    io_uring_prep_recvmsg(sqe, fd_, &s.msg, 0);
    sqe->user_data = pack_ud_slot(Op::RECV, slot);
    return true;
  }
  return false;
}

bool UringDriver::submit_send(uint32_t slot) {
  auto &s = udp_[slot];
  std::cout << "SENDING" << std::endl;

  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    s.siov.iov_base = s.out.data();
    s.siov.iov_len = s.out.size();

    std::memset(&s.smsg, 0, sizeof(s.smsg));
    s.smsg.msg_name = &s.peer;
    s.smsg.msg_namelen = sizeof(s.peer);
    s.smsg.msg_iov = &s.siov;
    s.smsg.msg_iovlen = 1;

    io_uring_prep_sendmsg(sqe, fd_, &s.smsg, 0);
    sqe->user_data = pack_ud_slot(Op::SEND, slot);

    return true;
  }
  return false;
}

bool UringDriver::submit_close(int fd) {
  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    io_uring_prep_close(sqe, fd);
    sqe->user_data = pack_ud_slot(Op::CLOSE, fd);
    return true;
  }
  return false;
}

struct Vec2f {
  float x;
  float y;
};

void print_vec2f(const char *buf, int n) {
  Vec2f v{};
  std::memcpy(&v, buf, sizeof(v));
  std::cout << "as float x: " << v.x << " y: " << v.y;
}

struct Players {
  std::uint32_t op; // 0 register 1 player update
  std::uint32_t id;
  float x;
  float y;
};

std::unordered_map<uint32_t, int> players_list;

void UringDriver::recv(uint32_t slot, int res) {
  auto &s = udp_[slot];
  if (res < 0) {
    std::cerr << "RECV(slot=" << slot << ") err=" << strerror(-res) << " ( "
              << res << ")\n";
    submit_recv(slot);
    io_uring_submit(&ring_);
    return;
  }

  std::cout << "RECV slot=" << slot << " bytes=" << res << " data='";
  if (res == (int)sizeof(Players)) {
    Players p{};
    std::memcpy(&p, s.buf, sizeof(Players));
    if (p.op == 0)
      players_list[p.id] = slot;
    std::cout << p.op << " " << p.id << " " << p.x << " " << p.y << "\n";

  } else {
    std::cout << "Unexpected size" << "\n";
  }
  // std::cout << "'\n";
  // std::cout.flush();
  // dump_hex(s.buf, res);

  s.out.assign(s.buf, s.buf + res);
  submit_send(slot);
  io_uring_submit(&ring_);
}

void UringDriver::send(uint32_t slot, int res) {
  auto &s = udp_[slot];

  if (res < 0) {
    std::cerr << "SEND error: " << strerror(-res) << " (" << res << ")\n";
  }

  s.out.clear();

  submit_recv(slot);
  io_uring_submit(&ring_);
}

void UringDriver::run() {
  std::cout << "RUNNING" << std::endl;
  while (!g_stop) {
    io_uring_cqe *cqe{};
    int rc = io_uring_wait_cqe(&ring_, &cqe);
    if (rc < 0) {
      if (rc == -EINTR)
        continue;
      std::cerr << "io_uring_wait_cqe: " << strerror(-rc) << "\n";
      break;
    }

    uint64_t ud = cqe->user_data;
    Op op = unpack_op_slot(ud);
    uint32_t slot = unpack_slot(ud);
    int res = cqe->res;

    io_uring_cqe_seen(&ring_, cqe);

    switch (op) {
    case Op::RECV:
      recv(slot, res);
      break;
    case Op::SEND:
      send(slot, res);
      break;
    case Op::CLOSE:
      io_uring_submit(&ring_);
      break;
    }
  }
}

UringDriver::~UringDriver() {
  if (fd_ >= 0)
    ::close(fd_);
  io_uring_queue_exit(&ring_);
}
