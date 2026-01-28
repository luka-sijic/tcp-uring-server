#include "net/uring_driver.hpp"

#include <cstring>
#include <iostream>
#include <signal.h>

#include "core/parser.hpp"
#include "models/response.h"

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
    throw std::runtime_error("io_uring_queue_init failed");
  }

  std::unordered_map<int, Conn> conns;

  refill_accepts();
  io_uring_submit(&ring_);
}

bool UringDriver::submit_accept(int listen_fd) {
  // we get a SQE from our buffer ring
  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    // fills out the SQE
    io_uring_prep_accept(sqe, listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
    sqe->user_data = pack_ud(Op::ACCEPT, listen_fd);
    return true;
  }
  return false;
}

void UringDriver::refill_accepts() {
  while (pending_accepts_ < kAcceptPipeline) {
    if (!submit_accept(fd_)) {
      accept_refill_needed_ = true;
      return;
    }
    ++pending_accepts_;
  }
  accept_refill_needed_ = false;
}

bool UringDriver::submit_recv(Conn &c) {
  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    io_uring_prep_recv(sqe, c.fd, c.buf, Conn::kBufSize, 0);
    sqe->user_data = pack_ud(Op::RECV, c.fd);
    return true;
  }
  return false;
}

bool UringDriver::submit_send(Conn &c) {
  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    const char *p = c.out.data() + c.out_sent;
    size_t n = c.out.size() - c.out_sent;

    io_uring_prep_send(sqe, c.fd, p, n, 0);
    sqe->user_data = pack_ud(Op::SEND, c.fd);

    return true;
  }
  return false;
}

bool UringDriver::submit_close(int fd) {
  if (io_uring_sqe *sqe = io_uring_get_sqe(&ring_)) {
    io_uring_prep_close(sqe, fd);
    sqe->user_data = pack_ud(Op::CLOSE, fd);
    return true;
  }
  return false;
}

void UringDriver::accept(int res) {
  if (pending_accepts_ > 0) {
    --pending_accepts_;
  }

  if (res < 0) {
    std::cerr << "[accept] error: " << strerror(-res) << " (" << res << ")\n";
    refill_accepts();
    io_uring_submit(&ring_);
    return;
  }

  int client_fd = res;

  auto [it, inserted] = conns_.try_emplace(client_fd);
  Conn &c = it->second;
  c.fd = client_fd;
  c.in.clear();
  c.out.clear();
  c.out_sent = 0;

  // std::cout << "Client Connected\n";

  // Post initial recv and top up accept pipeline.
  submit_recv(c);
  refill_accepts();
  io_uring_submit(&ring_);
}

void UringDriver::recv(int fd, int res) {
  auto it = conns_.find(fd);
  if (it == conns_.end())
    return;

  if (res <= 0) {
    submit_close(fd);
    conns_.erase(it);
    io_uring_submit(&ring_);
    return;
  }

  Conn &c = it->second;
  c.in.append(c.buf, c.buf + res);
  // std::cout << c.in << std::endl;

  size_t request_end = c.in.find("\r\n\r\n");
  if (request_end == std::string::npos) {
    submit_recv(c);
    io_uring_submit(&ring_);
    return;
  }

  std::string_view request_view(c.in.data(), request_end + 4);
  HttpRequest req{};
  if (!Parser::parse(request_view, req)) {
    submit_recv(c);
    io_uring_submit(&ring_);
    return;
  }
  // std::cout << "Method: " << req.method << " " << req.path << std::endl;

  std::string m{req.method};
  std::string p{req.path};
  auto result = router_.match(m, p);
  c.out = result();
  submit_send(c);

  c.in.erase(0, request_end + 4);

  io_uring_submit(&ring_);
}

void UringDriver::send(int fd, int res) {
  auto it = conns_.find(fd);
  if (it == conns_.end())
    return;

  if (res < 0) {
    submit_close(fd);
    conns_.erase(it);
    io_uring_submit(&ring_);
    return;
  }

  Conn &c = it->second;
  c.out_sent += (size_t)res;
  if (c.out_sent < c.out.size()) {
    submit_send(c);
    io_uring_submit(&ring_);
    return;
  }

  c.out.clear();
  c.out_sent = 0;
  submit_recv(c);
  io_uring_submit(&ring_);
  return;
}

void UringDriver::run() {
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
    Op op = unpack_op(ud);
    int fd = unpack_fd(ud);
    int res = cqe->res;

    io_uring_cqe_seen(&ring_, cqe);

    switch (op) {
    case Op::ACCEPT:
      accept(res);
      break;
    case Op::RECV:
      recv(fd, res);
      // send(fd, res);
      break;
    case Op::SEND:
      send(fd, res);
      break;
    case Op::CLOSE:
      if (accept_refill_needed_) {
        refill_accepts();
        io_uring_submit(&ring_);
      }
      break;
    }

    if (accept_refill_needed_) {
      refill_accepts();
      io_uring_submit(&ring_);
    }
  }
}

UringDriver::~UringDriver() {
  for (auto &[fd, _] : conns_)
    ::close(fd);
  if (fd_ >= 0)
    ::close(fd_);
  io_uring_queue_exit(&ring_);
}
