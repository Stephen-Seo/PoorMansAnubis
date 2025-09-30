#include "../src/http.h"

#include <print>
#include <cstring>
#include <thread>

#include <unistd.h>
#include <poll.h>

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

  // Do write.
  std::println("Start write request");
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ssize_t ret_write = write(socket_fd, "GET / HTTP/1.1\n\n", 16);
    if (ret_write == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      } else {
        close(socket_fd);
        socket_fd = -1;
        std::println(stderr, "ERROR: Failed to write to socket, errno {}", errno);
        break;
      }
    } else {
      break;
    }
  }

  // Read response.
  std::println("Start read response");
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    struct pollfd pfd = {socket_fd, POLLIN, 0};
    int ret = poll(&pfd, 1, /* milliseconds */ 1000);
    if (ret == -1) {
      close(socket_fd);
      socket_fd = -1;
      std::println("Error while polling socket_fd, errno {}", errno);
      break;
    } else if ((pfd.revents & POLLHUP) != 0 || (pfd.revents & POLLERR) != 0) {
      std::println("POLLHUP | POLLERR");
      close(socket_fd);
      socket_fd = -1;
      break;
    } else if ((pfd.revents & POLLIN) != 0) {
      std::println("POLLIN");
      std::array<char, 1024> buf{0};
      ssize_t read_ret = read(socket_fd, buf.data(), buf.size() - 1);
      if (read_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        } else {
          close(socket_fd);
          socket_fd = -1;
          std::println("Error reading from socket, errno {}", errno);
          break;
        }
      } else if (read_ret > 0) {
        buf.at(read_ret) = 0;
        std::println("Read: {}", buf.data());
      } else if (read_ret == 0) {
        std::println("EOF");
        close(socket_fd);
        socket_fd = -1;
        break;
      }
    }
  }

  std::println("End of loop...");
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  if (socket_fd >= 0) {
    close(socket_fd);
  }
  return 0;
}
