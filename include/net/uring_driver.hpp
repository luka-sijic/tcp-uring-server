#pragma once

#include <liburing.h>
#include <unordered_map>

#include "core/helpers.h"
#include "core/router.hpp"
#include "net/connection.h"

class UringDriver {
public:
    UringDriver(Router* r, int fd);
    ~UringDriver();

    bool submit_accept(int listen_fd);
    bool submit_multi(int listen_fd);
    bool submit_recv(Conn& c);
    bool submit_send(Conn& c);
    bool submit_close(int fd);

    void accept(io_uring_cqe*cqe, int res);
    void recv(int fd, int res);
    void send(int fd, int res);

    void run();
private:
    io_uring ring_{};
    int fd_{-1};
    Router router_;
    std::unordered_map<int, Conn> conns_;
};