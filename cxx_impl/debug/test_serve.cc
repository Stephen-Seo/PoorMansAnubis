#include "../src/http.h"

#include <thread>
#include <chrono>
#include <print>
#include <unordered_set>
#include <cstring>
#include <cstdlib>

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

void print_usage() {
  std::println("./program ( --ipv4=0.0.0.0 | --ipv6=:: ) --port=9000");
}

int main(int argc, char **argv) {
  int socket_fd = -1;

  bool is_ipv6 = false;

  if (argc != 3) {
    print_usage();
    return 0;
  }

  uint16_t port = 0;

  if (std::strncmp(argv[2], "--port=", 7) == 0) {
    int to_port = std::atoi(argv[2] + 7);
    if (to_port <= 0 || to_port > 0xFFFF) {
      std::println("Invalid --port={} !", to_port);
      print_usage();
      return 1;
    }
    port = static_cast<uint16_t>(to_port);
  } else {
    std::println("Expected --port=... for second argument.");
    print_usage();
    return 1;
  }

  std::println("Using port {}", port);

  if (std::strncmp(argv[1], "--ipv4=", 7) == 0) {
    std::println("Using ipv4 addr {}", argv[1] + 7);
    const auto [err_enum, err_str, ret_socket_fd] = PMA_HTTP::get_ipv4_socket_server(argv[1] + 7, port);

    if (err_enum != PMA_HTTP::ErrorT::SUCCESS) {
      std::println(stderr, "Error {}: {}", PMA_HTTP::error_t_to_str(err_enum), err_str);
      if (ret_socket_fd >= 0) {
        close(ret_socket_fd);
      }
      return 1;
    }
    socket_fd = ret_socket_fd;
    is_ipv6 = false;
  } else if (std::strncmp(argv[1], "--ipv6=", 7) == 0) {
    std::println("Using ipv6 addr {}", argv[1] + 7);

    const auto [err_enum, err_str, ret_socket_fd] = PMA_HTTP::get_ipv6_socket_server(argv[1] + 7, port);

    if (err_enum != PMA_HTTP::ErrorT::SUCCESS) {
      std::println(stderr, "Error {}: {}", PMA_HTTP::error_t_to_str(err_enum), err_str);
      if (ret_socket_fd >= 0) {
        close(ret_socket_fd);
      }
      return 1;
    }
    socket_fd = ret_socket_fd;
    is_ipv6 = true;
  } else {
    std::println("Expected --ipv4=... or --ipv6=... as first argument.");
    print_usage();
    return 0;
  }

  signal(SIGINT, handle_signal);
  signal(SIGHUP, handle_signal);

  std::unordered_set<int> connections;
  struct sockaddr_in addr_v4;
  struct sockaddr_in6 addr_v6;
  while (do_run) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    int ret;
    if (is_ipv6) {
      socklen_t socklen = sizeof(struct sockaddr_in6);
      ret = accept4(socket_fd, reinterpret_cast<sockaddr*>(&addr_v6), &socklen, SOCK_NONBLOCK);
    } else {
      socklen_t socklen = sizeof(struct sockaddr_in);
      ret = accept4(socket_fd, reinterpret_cast<sockaddr*>(&addr_v4), &socklen, SOCK_NONBLOCK);
    }
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
