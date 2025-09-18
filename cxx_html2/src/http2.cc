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

#include "http2.h"

// Local includes
#include "constants.h"

// Unix includes
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

// Standard Library includes
#include <cstring>
#include <format>

void ReceivingBuf::reset() {
  for (size_t idx = 0; idx < buf.size(); ++idx) {
    std::memset(buf[idx].data(), 0, buf[idx].size());
  }
  buf0_idx = 0;
  buf1_idx = 0;
  buf0_pending = 0;
  buf1_pending = 0;
  current_buf = 0;
}

Http2Server::Http2Server(uint16_t port, uint32_t scope_id)
    : socket_fd(socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 0)) {
  if (socket_fd == -1) {
    flags.set(0);
    errors.push_back("Failed to create socket");
    return;
  }

  // Bind socket to ipv6 address.
  {
    struct sockaddr_in6 sockaddr6;
    sockaddr6.sin6_family = AF_INET6;
    sockaddr6.sin6_port = port;
    sockaddr6.sin6_flowinfo = 0;
    sockaddr6.sin6_scope_id = scope_id;

    // Local "loopback" addr in ipv6 is "::1"
    std::memset(&sockaddr6.sin6_addr, 0, sizeof(struct in6_addr));
    sockaddr6.sin6_addr.s6_addr[15] = 1;

    if (bind(socket_fd, reinterpret_cast<const struct sockaddr *>(&sockaddr6),
             sizeof(struct sockaddr_in6)) == -1) {
      flags.set(0);
      errors.push_back("Failed to bind socket to address");
      return;
    }
  }

  // Set Socket to listen.
  {
    if (listen(socket_fd, LISTEN_SOCKET_BACKLOG_AMT) == -1) {
      flags.set(0);
      errors.push_back("Failed to set socket to listen");
      return;
    }
  }
}

Http2Server::~Http2Server() { close(socket_fd); }

void Http2Server::update() {
  if (flags.test(0)) {
    return;
  }

  // Accept connections on update frame.
  while (true) {
    ClientInfo cli_info{{0}, {}, -1};
    socklen_t len = sizeof(struct sockaddr_in6);
    int fd = accept4(socket_fd,
                     reinterpret_cast<struct sockaddr *>(&cli_info.addr_info),
                     &len, SOCK_NONBLOCK);

    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // No connections to receive as of this instant.
        break;
      }

      errors.push_back(
          std::format("Failed to accept connection: errno {}", errno));
      flags.set(1);
      break;
    }

    if (len != 16) {
      errors.push_back(std::format(
          "Failed to accept connection due to invalid addr size: errno {}",
          errno));
      flags.set(1);
      close(fd);
      break;
    }
  }

  // Handle existing connections.
  ReceivingBuf recv_buf;
  for (auto iter = connected_fds.begin(); iter != connected_fds.end(); ++iter) {
    recv_buf.reset();
    const auto [headers, error_enum] =
        Http2ServerHelpers::parse_headers(iter->fd, recv_buf);

    {
      bool do_continue = false;
      switch (error_enum) {
        case Http2Error::SUCCESS:
          // TODO handle this.
          break;
        case Http2Error::REACHED_EOF:
          iter->flags.set(2);
          break;
        case Http2Error::ERROR_READING:
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Nothing received.
            do_continue = true;
            break;
          } else {
            // Error during receiving.
          }
          break;
        case Http2Error::VALID_HTTP2_UPGRADE_REQ:
          // TODO handle this.
          break;
      }

      if (do_continue) {
        continue;
      }
    }
  }

  // Delete connections marked for deletion.
  for (size_t idx = connected_fds.size(); idx-- > 0;) {
    if (connected_fds.at(idx).flags.test(1)) {
      if (connected_fds[idx].fd != -1) {
        close(connected_fds[idx].fd);
        connected_fds[idx].fd = -1;
      }

      if (idx == connected_fds.size() - 1) {
        if (connected_fds[idx].fd != -1) {
          close(connected_fds[idx].fd);
          connected_fds[idx].fd = -1;
        }
        connected_fds.pop_back();
      } else {
        connected_fds.at(idx) =
            std::move(connected_fds.at(connected_fds.size() - 1));
        connected_fds.pop_back();
      }
    }
  }
}

std::deque<std::string> Http2Server::get_error() {
  std::deque<std::string> temp = std::move(errors);
  errors = std::deque<std::string>{};
  return temp;
}

std::tuple<std::list<std::string>, Http2Error>
Http2ServerHelpers::parse_headers(int fd, ReceivingBuf &recv_buf) {
  ssize_t read_ret =
      read(fd, recv_buf.buf.at(recv_buf.current_buf).data(), RECV_BUF_SIZE);

  if (read_ret == -1) {
  }

  // TODO Implement this.
  return {std::list<std::string>{}, Http2Error::SUCCESS};
}
