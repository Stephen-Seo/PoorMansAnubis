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

#ifndef SEODISPARATE_COM_PMA_CXX_HTTP2_H_
#define SEODISPARATE_COM_PMA_CXX_HTTP2_H_

// standard library includes
#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <list>
#include <string>
#include <tuple>

// Unix includes
#include <netinet/in.h>

// Local includes
#include "constants.h"

enum class Http2Error {
  SUCCESS = 0,
  REACHED_EOF = 1,
  ERROR_READING = 2,
  VALID_HTTP2_UPGRADE_REQ = 4
};

struct ReceivingBuf {
  std::array<std::array<char, RECV_BUF_SIZE>, 2> buf;
  size_t buf0_idx;
  size_t buf1_idx;
  size_t buf0_pending;
  size_t buf1_pending;
  size_t current_buf;

  void reset();
};

struct ClientInfo {
  struct sockaddr_in6 addr_info;
  // 0 - client initiated
  // 1 - marked for deletion
  // 2 - EOF
  std::bitset<32> flags;
  int fd;
};

class Http2Server {
 public:
  Http2Server(uint16_t port, uint32_t scope_id = 0);
  ~Http2Server();

  void update();

  std::deque<std::string> get_error();

 private:
  std::deque<ClientInfo> connected_fds;
  std::deque<std::string> errors;
  // 0 - is invalid
  // 1 - failed to accept a connection
  std::bitset<32> flags;
  int socket_fd;

  void update_connected();
};

namespace Http2ServerHelpers {
std::tuple<std::list<std::string>, Http2Error> parse_headers(
    int fd, ReceivingBuf &recv_buf);
}

#endif
