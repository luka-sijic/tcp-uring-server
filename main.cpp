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
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

#include "connection.h"
#include "protocol.h"
#include "sbbf.h"
#include "trace.h"

static constexpr unsigned QUEUE_DEPTH = 1024;
static constexpr size_t BUF_SIZE = 4096;

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int) { g_stop = 1; }

enum class Op : uint8_t { ACCEPT = 1, RECV = 2, SEND = 3, CLOSE = 4 };

static inline uint64_t pack_ud(Op op, int fd, uint32_t aux = 0) {
  return (uint64_t(op) << 56) | (uint64_t(uint32_t(fd) & 0x00FFFFFF) << 32) |
         uint64_t(aux);
}
static inline Op unpack_op(uint64_t ud) { return Op((ud >> 56) & 0xFF); }
static inline int unpack_fd(uint64_t ud) {
  return int((ud >> 32) & 0x00FFFFFF);
}
static inline uint32_t unpack_aux(uint64_t ud) {
  return uint32_t(ud & 0xFFFFFFFFu);
}

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
    perror("setsockopt(RO_REUSEADDR)");
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
static void submit_accept_simple(io_uring &ring, int listen_fd) {
  io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return;
  io_uring_prep_accept(sqe, listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
  sqe->user_data = pack_ud(Op::ACCEPT, listen_fd);
}

static void submit_recv(io_uring &ring, Conn &c) {
  io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return;
  io_uring_prep_recv(sqe, c.fd, c.buf, BUF_SIZE, 0);
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

int main(int argc, char **argv) {
  auto sbf = SBBF(1'000'000, .01);
  signal(SIGINT, on_sigint);

  uint16_t port = 9000;
  if (argc >= 2)
    port = static_cast<uint16_t>(std::stoi(argv[1]));

  int listen_fd = make_listen_socket(port);
  if (listen_fd < 0)
    return 1;

  io_uring ring;
  if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
    perror("io_uring_queue_init");
    ::close(listen_fd);
    return 1;
  }

  std::unordered_map<int, Conn> conns;

  for (int i = 0; i < 64; i++)
    submit_accept_simple(ring, listen_fd);
  io_uring_submit(&ring);

  std::cout << "Listening on 0.0.0.0:" << port << " (Ctrl+C to stop)\n";

  while (!g_stop) {
    io_uring_cqe *cqe = nullptr;
    int rc = io_uring_wait_cqe(&ring, &cqe);
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

    io_uring_cqe_seen(&ring, cqe);

    if (op == Op::ACCEPT) {
      submit_accept_simple(ring, listen_fd);

      if (res < 0) {
        std::cerr << "[accept] error: " << strerror(-res) << " (" << res
                  << ")\n";
        io_uring_submit(&ring);
        // transient errors can happen; keep going
        // // -EAGAIN if nonblocking and no pending connections
        continue;
      }

      int client_fd = res;

      Conn c;
      c.fd = client_fd;
      conns.emplace(client_fd, c);

      submit_recv(ring, conns.at(client_fd));
      io_uring_submit(&ring);
      continue;
    }
    if (op == Op::RECV) {
      auto it = conns.find(fd);
      if (it == conns.end())
        continue;

      if (res <= 0) {
        submit_close(ring, fd);
        conns.erase(it);
        io_uring_submit(&ring);
        continue;
      }

      Conn &c = it->second;
      c.in.append(c.buf, c.buf + res);

      while (true) {
        TRACE("INSERTED %s", "TEST");
        size_t nl = c.in.find('\n');
        if (nl == std::string::npos)
          break;

        std::string line = c.in.substr(0, nl);
        c.in.erase(0, nl + 1);

        protocol::handle_line(sbbf, line, c.out);
      }
      if (!c.out.empty() && c.out_sent == 0) {
        submit_send(ring, c);
      } else {
        // otherwise keep receiving
        submit_recv(ring, c);
      }

      io_uring_submit(&ring);
      continue;
    }
    if (op == Op::SEND) {
      auto it = conns.find(fd);
      if (it == conns.end())
        continue;

      if (res < 0) {
        submit_close(ring, fd);
        conns.erase(it);
        io_uring_submit(&ring);
        continue;
      }

      Conn &c = it->second;
      c.out_sent += (size_t)res;

      if (c.out_sent < c.out.size()) {
        // partial send
        submit_send(ring, c);
      } else {
        // finished sending; clear output and resume recv
        c.out.clear();
        c.out_sent = 0;
        submit_recv(ring, c);
      }
      io_uring_submit(&ring);
      continue;
    }

    if (op == Op::CLOSE) {
      io_uring_submit(&ring);
      continue;
    }
  }
  for (auto &[fd, _] : conns)
    ::close(fd);
  ::close(listen_fd);
  io_uring_queue_exit(&ring);
  return 0;
}
