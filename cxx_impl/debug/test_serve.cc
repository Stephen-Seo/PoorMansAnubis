#include "../src/http.h"

#include <thread>
#include <chrono>
#include <print>
#include <unordered_set>

#include <signal.h>
#include <errno.h>

#include <netinet/in.h>

static bool do_run = true;

void handle_signal(int sig) {
  do_run = false;
}

std::unordered_set<int> update_connections(const std::unordered_set<int> &connections) {
  std::unordered_set<int> to_remove;

  std::array<uint8_t, 1024> buf;
  for (int fd : connections) {
    ssize_t read_ret = read(fd, buf.data(), buf.size() - 1);
    if (read_ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {}
      else {
        std::println("ERROR: read on fd {} returned -1 with errno {}!", fd, errno);
        to_remove.insert(fd);
      }
    } else if (read_ret > 0) {
      buf.at(read_ret) = 0;
      std::println("READ fd {}: {}", fd, reinterpret_cast<char*>(buf.data()));
      to_remove.insert(fd);
    }
  }
  return to_remove;
}

int main() {
  const auto [err_enum, err_str, socket_fd] = PMA_HTTP::get_ipv4_socket("127.0.0.1", 9000);

  if (err_enum != PMA_HTTP::ErrorT::SUCCESS) {
    std::println(stderr, "Error {}: {}", PMA_HTTP::error_t_to_str(err_enum), err_str);
    if (socket_fd >= 0) {
      close(socket_fd);
    }
    return 1;
  }

  signal(SIGINT, handle_signal);
  signal(SIGHUP, handle_signal);

  std::unordered_set<int> connections;
  struct sockaddr_in addr_v4;
  while (do_run) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    socklen_t socklen = sizeof(struct sockaddr_in);
    int ret = accept4(socket_fd, reinterpret_cast<sockaddr*>(&addr_v4), &socklen, SOCK_NONBLOCK);
    if (ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {}
      else {
        std::println("ERROR: accept returned -1 with errno {}!", errno);
        do_run = false;
      }
    } else {
      connections.insert(ret);
    }

    auto to_close = update_connections(connections);
    for (int fd : to_close) {
      std::println("Closing connection {}...", fd);
      close(fd);
      connections.erase(fd);
    }
  }

  close(socket_fd);

  return 0;
}
