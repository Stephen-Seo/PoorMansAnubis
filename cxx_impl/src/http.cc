// ISC License
//
// Copyright (c) 2025 Stephen Seo
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include "http.h"

// Unix includes
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

// Standard library includes
#include <cstring>
#include <format>

// Local includes
#include "helpers.h"

std::string PMA_HTTP::error_t_to_str(PMA_HTTP::ErrorT err_enum) {
  switch (err_enum) {
    case ErrorT::SUCCESS:
      return "SUCCESS";
    case ErrorT::FAILED_TO_GET_IPV6_SOCKET:
      return "FailedToGetIPV6Socket";
    case ErrorT::FAILED_TO_GET_IPV4_SOCKET:
      return "FailedToGetIPV4Socket";
    default:
      return "UnknownError";
  }
}

std::tuple<PMA_HTTP::ErrorT, std::string, int> PMA_HTTP::get_ipv6_socket(
    std::string addr, uint16_t port) {
  int socket_fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
            std::format("Failed to create socket, errno {}", errno), 0};
  }

  // bind to [::], with port
  {
    struct sockaddr_in6 sain6;
    sain6.sin6_family = AF_INET6;
    sain6.sin6_port = PMA_HELPER::be_swap_u16(port);
    sain6.sin6_flowinfo = 0;
    std::memset(sain6.sin6_addr.s6_addr, 0, 16);
    sain6.sin6_scope_id = 0;

    int ret = bind(socket_fd, reinterpret_cast<const sockaddr*>(&sain6),
                   sizeof(struct sockaddr_in6));
    if (ret != 0) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
              std::format("Failed to bind socket, errno {}", errno), 0};
    }
  }

  // set enable listen
  {
    int ret = listen(socket_fd, SOCKET_BACKLOG_SIZE);
    if (ret == -1) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
              std::format("Failed to set socket to listen, errno {}", errno),
              0};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}

std::tuple<PMA_HTTP::ErrorT, std::string, int> PMA_HTTP::get_ipv4_socket(
    std::string addr, uint16_t port) {
  int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
            std::format("Failed to create socket, errno {}", errno), 0};
  }

  // bind to 0.0.0.0, with port
  {
    struct sockaddr_in sain;
    sain.sin_family = AF_INET;
    sain.sin_port = PMA_HELPER::be_swap_u16(port);
    sain.sin_addr.s_addr = 0;

    int ret = bind(socket_fd, reinterpret_cast<const sockaddr*>(&sain),
                   sizeof(struct sockaddr_in));
    if (ret != 0) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
              std::format("Failed to bind socket, errno {}", errno), 0};
    }
  }

  // set enable listen
  {
    int ret = listen(socket_fd, SOCKET_BACKLOG_SIZE);
    if (ret == -1) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
              std::format("Failed to set socket to listen, errno {}", errno),
              0};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}
