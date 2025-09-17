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

Http2Server::Http2Server(uint16_t port, uint32_t scope_id)
    : socket_fd(socket(AF_INET6, SOCK_STREAM, 0)) {
  // non-blocking io can be set per "accept" call.

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

    if (bind(socket_fd, reinterpret_cast<const struct sockaddr*>(&sockaddr6),
             sizeof(struct sockaddr_in6)) == -1) {
      flags.set(0);
      errno_cached = errno;
      return;
    }
  }

  // Set Socket to listen.
  {
    if (listen(socket_fd, LISTEN_SOCKET_BACKLOG_AMT) == -1) {
      flags.set(0);
      errno_cached = errno;
      return;
    }
  }
}

Http2Server::~Http2Server() { close(socket_fd); }

void Http2Server::update() {
  // Accept connections on update frame.
  // TODO
}

std::optional<std::string> Http2Server::get_error() {
  if (errno_cached.has_value()) {
    int errno_cached = this->errno_cached.value();
    this->errno_cached.reset();
    return std::format("Got errno {}", errno_cached);
  }
  return std::nullopt;
}
