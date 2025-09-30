#include "../src/http.h"

#include <print>
#include <cstring>
#include <thread>

#include <unistd.h>

void print_usage() {
  std::println("./program ( --cli-ipv4=0.0.0.0 | --cli-ipv6=:: ) ( --ser-ipv4=0.0.0.0 | --ser-ipv6=:: ) --port=9000");
}

int main(int argc, char **argv) {
  int socket_fd = -1;

  if (argc != 4) {
    print_usage();
    return 0;
  }

  uint16_t port = 0;

  if (std::strncmp(argv[3], "--port=", 7) == 0) {
    port = std::atoi(argv[3] + 7);
  } else {
    std::println("Expected --port=... for second argument.");
    print_usage();
    return 1;
  }

  std::println("Using port {}", port);

  if (std::strncmp(argv[1], "--cli-ipv4=", 11) == 0 && std::strncmp(argv[2], "--ser-ipv4=", 11) == 0) {
    std::println("Using ipv4 addr {}", argv[1] + 11);
    const auto [err_enum, err_str, ret_socket_fd] = PMA_HTTP::connect_ipv4_socket_client(argv[1] + 11, argv[2] + 11, port);

    if (err_enum != PMA_HTTP::ErrorT::SUCCESS || ret_socket_fd < 0) {
      std::println(stderr, "Error {}: {}", PMA_HTTP::error_t_to_str(err_enum), err_str);
      if (ret_socket_fd >= 0) {
        close(ret_socket_fd);
      }
      return 1;
    }
    socket_fd = ret_socket_fd;
  } else if (std::strncmp(argv[1], "--cli-ipv6=", 11) == 0 && std::strncmp(argv[2], "--ser-ipv6=", 11) == 0) {
    std::println("Using ipv6 addr {}", argv[1] + 11);

    const auto [err_enum, err_str, ret_socket_fd] = PMA_HTTP::connect_ipv6_socket_client(argv[1] + 11, argv[2] + 11, port);

    if (err_enum != PMA_HTTP::ErrorT::SUCCESS) {
      std::println(stderr, "Error {}: {}", PMA_HTTP::error_t_to_str(err_enum), err_str);
      if (ret_socket_fd >= 0) {
        close(ret_socket_fd);
      }
      return 1;
    }
    socket_fd = ret_socket_fd;
  } else {
    std::println("Expected --ipv4=... or --ipv6=... as first argument.");
    print_usage();
    return 0;
  }

  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ssize_t ret_write = write(socket_fd, "GET / HTTP/1.1\n\n", 16);
    if (ret_write == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      } else {
        close(socket_fd);
        std::println(stderr, "ERROR: Failed to write to socket, errno {}", errno);
        break;
      }
    } else {
      break;
    }
  }

  std::println("End of loop...");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  close(socket_fd);
  return 0;
}
