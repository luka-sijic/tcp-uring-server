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
#include "core/trace.h"
#include "models/response.h"
#include "net/connection.h"
#include "net/server.hpp"
#include "net/uring_driver.hpp"

int Server::make_listen_socket(uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  TRACE("starting udp server");
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

  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0) {
    perror("setsockopt(SO_REUSEPORT)");
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

  return fd;
}

Server::Server(Router *r, uint16_t port) : router_(r), port_(port) {
  int fd = make_listen_socket(port);
  UringDriver driver(r, fd);
  driver.run();
  std::cout << "Listening on 0.0.0.0:" << port << " (Ctrl+C to stop)\n";
}

/*
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
        io_uring_submit(&impl_->driver->ring);
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
      std::string full;
      while (true) {
        // TRACE("INSERTED %s", "TEST");
        size_t nl = c.in.find('\n');
        if (nl == std::string::npos)
          break;

        std::string line = c.in.substr(0, nl);
        c.in.erase(0, nl + 1);

        //std::cout << line << std::endl;
        HttpRequest *req = new HttpRequest{};
        full += line;

        auto res = protocol::handle_line(line, *req);
        //std::cout << "METHOD: " << req->method << " target: " << req->target;
      }
      if (!c.out.empty() && c.out_sent == 0) {
        std::cout << full << std::endl;
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
*/
