#include "net/uring_driver.hpp"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <signal.h>

#include "core/helpers.h"
#include "models/net.h"

enum class OP { REGISTER, UPDATE, BROADCAST, DEREGISTER };

static bool same_endpoint(const sockaddr_storage &a, socklen_t alen,
                          const sockaddr_storage &b, socklen_t blen) {
  if (a.ss_family != b.ss_family)
    return false;

  if (a.ss_family == AF_INET) {
    if (alen < sizeof(sockaddr_in) || blen < sizeof(sockaddr_in))
      return false;
    auto *x = (const sockaddr_in *)&a;
    auto *y = (const sockaddr_in *)&b;
    return x->sin_port == y->sin_port &&
           x->sin_addr.s_addr == y->sin_addr.s_addr;
  }

  if (a.ss_family == AF_INET6) {
    if (alen < sizeof(sockaddr_in6) || blen < sizeof(sockaddr_in6))
      return false;
    auto *x = (const sockaddr_in6 *)&a;
    auto *y = (const sockaddr_in6 *)&b;
    return x->sin6_port == y->sin6_port &&
           std::memcmp(&x->sin6_addr, &y->sin6_addr, sizeof(in6_addr)) == 0;
  }

  return false;
}

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

UringDriver::UringDriver(int fd) : fd_(fd), router_(*this) {
  signal(SIGINT, on_sigint);

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

bool UringDriver::submit_recv(uint32_t slot) noexcept {
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

bool UringDriver::submit_send(uint32_t slot) noexcept {
  auto &s = udp_[slot];
  // std::cout << "SENDING" << std::endl;

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

bool UringDriver::submit_close(int fd) noexcept {
  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    io_uring_prep_close(sqe, fd);
    sqe->user_data = pack_ud_slot(Op::CLOSE, fd);
    return true;
  }
  return false;
}

void UringDriver::send_to(const sockaddr_storage &dst, socklen_t dst_len,
                          const void *data, size_t len) noexcept {
  if (len > SendState::kMax)
    return;

  uint32_t sidx = 0;
  SendState *ss = acquire_send_slot(sidx);
  if (!ss)
    return;

  ss->len = len;
  std::memcpy(ss->buf.data(), data, len);

  ss->iov.iov_base = ss->buf.data();
  ss->iov.iov_len = ss->len;

  ss->dst = dst;
  ss->dst_len = dst_len;

  std::memset(&ss->msg, 0, sizeof(ss->msg));
  ss->msg.msg_name = &ss->dst;
  ss->msg.msg_namelen = ss->dst_len;
  ss->msg.msg_iov = &ss->iov;
  ss->msg.msg_iovlen = 1;

  // Queue sendmsg
  io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
  io_uring_prep_sendmsg(sqe, fd_, &ss->msg, 0);

  // Tag completion so your CQE handler knows it's a SEND for this slot
  sqe->user_data = pack_ud_slot(Op::SEND, sidx);
}

SendState *UringDriver::acquire_send_slot(uint32_t &idx_out) noexcept {
  for (uint32_t n = 0; n < kSendSlots; n++) {
    uint32_t i = (send_rr_ + n) % kSendSlots;
    if (!send_[i].busy) {
      send_[i].busy = true;
      send_rr_ = (i + 1) % kSendSlots;
      idx_out = i;
      return &send_[i];
    }
  }
  return nullptr;
}

void UringDriver::on_send_complete(uint32_t send_idx, int res) noexcept {
  (void)res;
  send_[send_idx].busy = false;
}

void UringDriver::recv(uint32_t slot, int res) noexcept {
  auto &s = udp_[slot];
  if (res < 0) {
    std::cerr << "RECV(slot=" << slot << ") err=" << strerror(-res) << " ( "
              << res << ")\n";
    submit_recv(slot);
    io_uring_submit(&ring_);
    return;
  }

  // slot?
  // res = size
  // s.buf = actual data
  std::span<const std::byte> bytes =
      std::as_bytes(std::span{s.buf, static_cast<size_t>(res)});
  PacketView pkt{s.peer, s.peer_len, bytes};
  router_.on_packet(pkt);

  submit_recv(slot);
  io_uring_submit(&ring_);
}

void UringDriver::send(uint32_t slot, int res) noexcept {
  auto &s = udp_[slot];

  if (res < 0) {
    std::cerr << "SEND error: " << strerror(-res) << " (" << res << ")\n";
  }

  io_uring_submit(&ring_);
}

void UringDriver::start() noexcept {
  std::cerr << "Server is running on port 9000" << '\n';
  std::cerr.flush();
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
