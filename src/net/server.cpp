#include <liburing.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <expected>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/helpers.h"
#include "core/protocol.h"
#include "core/sbbf.h"
#include "core/trace.h"
#include "net/connection.h"
#include "net/server.h"

static constexpr unsigned kQueueDepth = 1024;
static constexpr size_t kAcceptPipeline = 4096;

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

static auto sbbf = SBBF(10'000'000, .01);

static int make_listen_socket(uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  TRACE("hello");
  if (fd < 0) {
    perror("socket");
    return -1;
  }

  int one = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    perror("setsockopt(SO_REUSEADDR)");
    ::close(fd);
    return -1;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    ::close(fd);
    return -1;
  }

  if (::listen(fd, SOMAXCONN) < 0) {
    perror("listen");
    ::close(fd);
    return -1;
  }
  return fd;
}
/*
static void submit_accept(io_uring &ring, int listen_fd) {
  io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return;

  auto *client_addr = new sockaddr_in{};
  auto *client_len = new socklen_t(sizeof(sockaddr_in));

  io_uring_prep_accept_direct(sqe, listen_fd, (sockaddr *)client_addr,
                              client_len, SOCK_NONBLOCK);
  sqe->user_data =
      pack_ud(Op::ACCEPT, listen_fd,
              reinterpret_cast<uintptr_t>(client_addr) & 0xFFFFFFFFu);
}
*/

struct Server::Impl {
  uint16_t port{};
  int listen_fd{-1};
  io_uring ring{};
  std::unordered_map<int, Conn> conns;
  std::unordered_map<std::string, std::string> m1;
  SBBF sbbf{1'000'000, .01};

  explicit Impl(uint16_t p) : port(p) {}
};

static void submit_accept_simple(io_uring &ring, int listen_fd) {
  // we get a SQE from our buffer ring
  io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return;
  // fills out the SQE
  io_uring_prep_accept(sqe, listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
  sqe->user_data = pack_ud(Op::ACCEPT, listen_fd);
}

static void submit_recv(io_uring &ring, Conn &c) {
  io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return;
  io_uring_prep_recv(sqe, c.fd, c.buf, Conn::kBufSize, 0);
  sqe->user_data = pack_ud(Op::RECV, c.fd);
}

static void submit_send(io_uring &ring, Conn &c) {
  io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return;
  const char *p = c.out.data() + c.out_sent;
  const size_t n = c.out.size() - c.out_sent;

  io_uring_prep_send(sqe, c.fd, p, n, 0);
  sqe->user_data = pack_ud(Op::SEND, c.fd);
}

static void submit_close(io_uring &ring, int fd) {
  io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return;
  io_uring_prep_close(sqe, fd);
  sqe->user_data = pack_ud(Op::CLOSE, fd);
}

static void print_payload(const char *p, size_t n) {
  std::string out;
  out.reserve(n);
  for (size_t i = 0; i < n; i++) {
    unsigned char ch = static_cast<unsigned char>(p[i]);
    out.push_back((ch >= 32 && ch <= 126) ? char(ch) : '.');
  }
  std::cout << "[recv] " << out << "\n";
}

Server::Server(uint16_t port) : impl_(new Impl(port)) {
  signal(SIGINT, on_sigint);

  impl_->listen_fd = make_listen_socket(port);
  if (impl_->listen_fd < 0)
    throw ::std::runtime_error("failed to create listen socket");

  if (io_uring_queue_init(kQueueDepth, &impl_->ring, 0) < 0) {
    ::close(impl_->listen_fd);
    throw std::runtime_error("io_uring_queue_init failed");
  }

  std::unordered_map<int, Conn> conns;

  for (std::size_t i = 0; i < kAcceptPipeline; ++i)
    submit_accept_simple(impl_->ring, impl_->listen_fd);
  io_uring_submit(&impl_->ring);

  std::cout << "Listening on 0.0.0.0:" << port << " (Ctrl+C to stop)\n";
}

Server::~Server() {
  if (!impl_)
    return;

  for (auto &[fd, _] : impl_->conns)
    ::close(fd);
  if (impl_->listen_fd >= 0)
    ::close(impl_->listen_fd);
  io_uring_queue_exit(&impl_->ring);
}

void Server::run() {
  while (!g_stop) {
    io_uring_cqe *cqe = nullptr;
    int rc = io_uring_wait_cqe(&impl_->ring, &cqe);
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

    io_uring_cqe_seen(&impl_->ring, cqe);

    if (op == Op::ACCEPT) {
      submit_accept_simple(impl_->ring, impl_->listen_fd);

      if (res < 0) {
        std::cerr << "[accept] error: " << strerror(-res) << " (" << res
                  << ")\n";
        io_uring_submit(&impl_->ring);
        // transient errors can happen; keep going
        // // -EAGAIN if nonblocking and no pending connections
        continue;
      }

      int client_fd = res;

      Conn c;
      c.fd = client_fd;
      impl_->conns.emplace(client_fd, c);

      submit_recv(impl_->ring, impl_->conns.at(client_fd));
      io_uring_submit(&impl_->ring);
      continue;
    }
    if (op == Op::RECV) {
      auto it = impl_->conns.find(fd);
      if (it == impl_->conns.end())
        continue;

      if (res <= 0) {
        submit_close(impl_->ring, fd);
        impl_->conns.erase(it);
        io_uring_submit(&impl_->ring);
        continue;
      }

      Conn &c = it->second;
      c.in.append(c.buf, c.buf + res);

      while (true) {
        // TRACE("INSERTED %s", "TEST");
        size_t nl = c.in.find('\n');
        if (nl == std::string::npos)
          break;

        std::string line = c.in.substr(0, nl);
        c.in.erase(0, nl + 1);

        std::cout << line << std::endl;

        protocol::handle_line(sbbf, line, c.out);
      }
      if (!c.out.empty() && c.out_sent == 0) {
        submit_send(impl_->ring, c);
      } else {
        // otherwise keep receiving
        submit_recv(impl_->ring, c);
      }

      io_uring_submit(&impl_->ring);
      continue;
    }
    if (op == Op::SEND) {
      auto it = impl_->conns.find(fd);
      if (it == impl_->conns.end())
        continue;

      if (res < 0) {
        submit_close(impl_->ring, fd);
        impl_->conns.erase(it);
        io_uring_submit(&impl_->ring);
        continue;
      }

      Conn &c = it->second;
      c.out_sent += (size_t)res;

      if (c.out_sent < c.out.size()) {
        // partial send
        submit_send(impl_->ring, c);
      } else {
        // finished sending; clear output and resume recv
        c.out.clear();
        c.out_sent = 0;
        submit_recv(impl_->ring, c);
      }
      io_uring_submit(&impl_->ring);
      continue;
    }

    if (op == Op::CLOSE) {
      io_uring_submit(&impl_->ring);
      continue;
    }
  }
}
