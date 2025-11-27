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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_CXX_BACKEND_DB_MSQL_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_CXX_BACKEND_DB_MSQL_H_

#include <array>
#include <bitset>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace PMA_MSQL {

struct MSQLPacket {
  MSQLPacket();
  ~MSQLPacket();

  MSQLPacket(uint32_t, uint8_t, uint8_t *);

  // No copy
  MSQLPacket(const MSQLPacket &) = delete;
  MSQLPacket &operator=(const MSQLPacket &) = delete;

  // Allow move
  MSQLPacket(MSQLPacket &&);
  MSQLPacket &operator=(MSQLPacket &&);

  // Treat this as 3 bytes.
  uint32_t packet_length : 24;
  uint8_t seq;
  uint8_t *body;
};

class MSQLConnection {
 public:
  MSQLConnection();
  ~MSQLConnection();

  MSQLConnection(int fd, uint32_t connection_id);

  // No copy
  MSQLConnection(const MSQLConnection &) = delete;
  MSQLConnection &operator=(const MSQLConnection &) = delete;

  // Allow move
  MSQLConnection(MSQLConnection &&);
  MSQLConnection &operator=(MSQLConnection &&);

  bool is_valid() const;

  // Returns error code, 0 on success, 1 if is_valid() is false, 2 for other
  // error.
  int execute_stmt(const std::string &stmt);

  void close_stmt(uint32_t stmt_id);

 private:
  // 0 - invalid connection if set.
  std::bitset<32> flags;
  int fd;
  uint32_t connection_id;
};

// Copies "data" into the returned packet struct(s).
std::vector<MSQLPacket> create_packets(uint8_t *data, size_t data_size,
                                       uint8_t *seq);

std::optional<MSQLConnection> connect_msql(std::string addr, uint16_t port,
                                           std::string user, std::string pass,
                                           std::string dbname);

std::array<uint8_t, 20> msql_native_auth_resp(std::vector<uint8_t> seed,
                                              std::string pass);

// 0 on success, 1 if error. Second integer is bytes read.
std::tuple<int, size_t> handle_ok_pkt(uint8_t *data, size_t size);

void print_error_pkt(uint8_t *data, size_t size);

// Integer value and bytes read.
// Bytes read is 0 on error. Value is 0 on null.
std::tuple<uint64_t, uint_fast8_t> parse_len_enc_int(uint8_t *data);

}  // Namespace PMA_MSQL

#endif
