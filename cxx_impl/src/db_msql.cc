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

#include "db_msql.h"

// Unix includes.
#include <unistd.h>

// Standard library includes.
#include <cstdlib>
#include <cstring>

// Local includes.
#include "helpers.h"
#include "http.h"

PMA_MSQL::MSQLPacket::MSQLPacket() : packet_length(0), seq(0), body(nullptr) {}

PMA_MSQL::MSQLPacket::~MSQLPacket() {
  if (body) {
    delete[] body;
  }
}

PMA_MSQL::MSQLPacket::MSQLPacket(uint32_t len, uint8_t seq, uint8_t *data)
    : packet_length(len & 0xFFFFFF), seq(seq), body(data) {}

PMA_MSQL::MSQLPacket::MSQLPacket(MSQLPacket &&other) {
  this->packet_length = other.packet_length;
  this->seq = other.seq;
  this->body = other.body;

  other.body = nullptr;
}

PMA_MSQL::MSQLPacket &PMA_MSQL::MSQLPacket::operator=(
    PMA_MSQL::MSQLPacket &&other) {
  this->packet_length = other.packet_length;
  this->seq = other.seq;
  this->body = other.body;

  other.body = nullptr;

  return *this;
}

std::vector<PMA_MSQL::MSQLPacket> PMA_MSQL::create_packets(uint8_t *data,
                                                           size_t data_size,
                                                           uint8_t *seq) {
  std::vector<MSQLPacket> ret;

  size_t offset = 0;
  while (data_size != 0) {
    if (data_size >= 0xFFFFFF) {
      ret.emplace_back(0xFFFFFF, (*seq)++, new uint8_t[0xFFFFFF]);
      std::memcpy(ret.back().body, data + offset, 0xFFFFFF);
      offset += 0xFFFFFF;
      data_size -= 0xFFFFFF;

      if (data_size == 0) {
        ret.emplace_back(0, (*seq)++, nullptr);
      }
    } else {
      ret.emplace_back(static_cast<uint32_t>(data_size), (*seq)++,
                       new uint8_t[data_size]);
      std::memcpy(ret.back().body, data + offset, data_size);
      offset += data_size;
      data_size = 0;
    }
  }

  return ret;
}

PMA_MSQL::MSQLConnection::MSQLConnection() : flags() {}

PMA_MSQL::MSQLConnection::~MSQLConnection() {}

std::optional<PMA_MSQL::MSQLConnection> PMA_MSQL::connect_msql(
    std::string addr, uint16_t port, std::string user, std::string pass) {
  auto [errt, errm, fd] =
      PMA_HTTP::connect_ipv6_socket_client(addr, "::", port);
  if (errt != PMA_HTTP::ErrorT::SUCCESS) {
    std::tie(errt, errm, fd) =
        PMA_HTTP::connect_ipv4_socket_client(addr, "0.0.0.0", port);
    if (errt != PMA_HTTP::ErrorT::SUCCESS) {
      return std::nullopt;
    }
  }

  uint8_t buf[4096];

  ssize_t read_ret = read(fd, buf, 4096);
  if (read_ret == 0 || (read_ret == 1 && buf[0] == 0xFF)) {
    close(fd);
    return std::nullopt;
  }

  ssize_t idx = 0;
  // Protocol version.
  ++idx;

  // Server version.
  while (buf[idx] != 0 && idx < 4096) {
    ++idx;
  }
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Connection id.
  uint32_t connection_id = *reinterpret_cast<uint32_t *>(buf + idx);
  idx += 4;
  connection_id = PMA_HELPER::le_swap_u32(connection_id);
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Auth plugin data.
  idx += 8;
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Reserved byte.
  idx += 1;
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Server capabilities (1st part).
  uint16_t server_capabilities_1 = *reinterpret_cast<uint16_t *>(buf + idx);
  server_capabilities_1 = PMA_HELPER::le_swap_u16(server_capabilities_1);
  idx += 2;
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Server default collation.
  idx += 1;
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // status flags.
  idx += 2;
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Server capabilities (2nd part).
  uint16_t server_capabilities_2 = *reinterpret_cast<uint16_t *>(buf + idx);
  server_capabilities_2 = PMA_HELPER::le_swap_u16(server_capabilities_2);
  idx += 2;
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Plugin auth.
  if (server_capabilities_2 & 0x8) {
    idx += 1;
  } else {
    idx += 1;
  }
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // Filler
  idx += 6;
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // CLIENT_MYSQL or server_capabilities_3
  uint32_t server_capabilities_3 = 0;
  if (server_capabilities_1 & 1) {
    // filler
    idx += 4;
  } else {
    server_capabilities_3 = *reinterpret_cast<uint32_t *>(buf + idx);
    server_capabilities_3 = PMA_HELPER::le_swap_u32(server_capabilities_3);
    idx += 4;
  }
  if (idx >= read_ret || idx >= 4096) {
    close(fd);
    return std::nullopt;
  }

  // CLIENT_SECURE_CONNECTION
  // Documentation is missing for this, so parsing the initial handshake stops
  // here.

  // Response.
  PMA_HELPER::BinaryParts parts;

  // Client capabilities.
  uint8_t *cli_buf = new uint8_t[4];
  uint32_t *u32 = reinterpret_cast<uint32_t *>(cli_buf);
  *u32 = 0;
  u32[0] |= 1 | 8;

  *u32 = PMA_HELPER::le_swap_u32(*u32);

  parts.append(4, cli_buf);

  // Max packet size.
  cli_buf = new uint8_t[4];
  u32 = reinterpret_cast<uint32_t *>(cli_buf);
  *u32 = 0x1000000;

  *u32 = PMA_HELPER::le_swap_u32(*u32);

  parts.append(4, cli_buf);

  // character set and collation.
  cli_buf = new uint8_t[1];

  // TODO figure out what to set for the character set and collation byte.

  close(fd);
  return std::nullopt;
}
