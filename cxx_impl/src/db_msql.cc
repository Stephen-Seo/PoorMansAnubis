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
#include <errno.h>
#include <unistd.h>

// Standard library includes.
#include <chrono>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

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
  PMA_HTTP::ErrorT errt = PMA_HTTP::ErrorT::INVALID_STATE;
  std::string errm;
  int fd;
  try {
    std::tie(errt, errm, fd) =
        PMA_HTTP::connect_ipv6_socket_client(addr, "::", port);
  } catch (const std::exception &e) {
    try {
      std::tie(errt, errm, fd) =
          PMA_HTTP::connect_ipv4_socket_client(addr, "0.0.0.0", port);
    } catch (const std::exception &e) {
      std::fprintf(stderr,
                   "ERROR: Failed to set up client socket for msql client to "
                   "server connection (invalid address?)\n");
      return std::nullopt;
    }
  }
  if (errt != PMA_HTTP::ErrorT::SUCCESS) {
    std::fprintf(stderr,
                 "ERROR: Failed to set up client socket for msql client to "
                 "server connection (invalid address?)\n");
    return std::nullopt;
  }

  uint8_t buf[4096];

  ssize_t read_ret = 0;
  while (true) {
    read_ret = read(fd, buf, 4096);
    if (read_ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // NONBLOCKING nothing to read.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifndef NDEBUG
        std::printf(".");
#endif
        continue;
      } else {
        std::fprintf(stderr, "ERROR: Error occurred reading data (errno %d)\n",
                     errno);
        return std::nullopt;
      }
    } else if (read_ret == 0 || buf[0] == 0xFF) {
      close(fd);
      std::fprintf(stderr, "ERROR: Failed to connect to msql server!\n");
      return std::nullopt;
    } else {
#ifndef NDEBUG
      std::printf("\n");
#endif
      break;
    }
  }
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: read_ret: %zd\n", read_ret);
#endif

  uint32_t pkt_size = static_cast<uint32_t>(buf[0]) |
                      (static_cast<uint32_t>(buf[1]) << 8) |
                      (static_cast<uint32_t>(buf[2]) << 16);
#ifndef NDEBUG
  std::fprintf(stderr, "pkt_size: %" PRIu32 " (%x)\n", pkt_size, pkt_size);
#endif
  uint8_t sequence_id = buf[3];
#ifndef NDEBUG
  std::fprintf(stderr, "seq: %hhu\n", sequence_id);
#endif

  uint8_t *pkt_data = buf + 4;

  ssize_t idx = 0;
  // Protocol version.
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Protocol version: %hhu\n", pkt_data[idx]);
#endif
  ++idx;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server version.
  std::printf("NOTICE: Connecting to server, reported version: %s\n",
              pkt_data + idx);
  while (pkt_data[idx] != 0 && idx < 4092 && idx < pkt_size) {
    ++idx;
  }
  ++idx;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: idx after server version: %zd\n", idx);
#endif

  // Connection id.
  uint32_t connection_id = static_cast<uint32_t>(pkt_data[idx]) |
                           (static_cast<uint32_t>(pkt_data[idx + 1]) << 8) |
                           (static_cast<uint32_t>(pkt_data[idx + 2]) << 16) |
                           (static_cast<uint32_t>(pkt_data[idx + 3]) << 24);
  idx += 4;
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Connection id %" PRIu32 " (%#" PRIx32 ")\n",
               connection_id, connection_id);
#endif
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Auth plugin data.
  std::unique_ptr<uint8_t[]> auth_plugin_data = std::make_unique<uint8_t[]>(64);
  std::memcpy(auth_plugin_data.get(), pkt_data + idx, 8);
  idx += 8;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Reserved byte.
  idx += 1;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server capabilities (1st part).
  uint16_t server_capabilities_1 =
      *reinterpret_cast<uint16_t *>(pkt_data + idx);
  server_capabilities_1 = PMA_HELPER::le_swap_u16(server_capabilities_1);
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Server capabilities 1: %#hx\n",
               server_capabilities_1);
#endif
  idx += 2;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server default collation.
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Server default collation: %hhu (%#hhx)\n",
               pkt_data[idx], pkt_data[idx]);
#endif
  idx += 1;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // status flags.
  idx += 2;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server capabilities (2nd part).
  uint16_t server_capabilities_2 =
      *reinterpret_cast<uint16_t *>(pkt_data + idx);
  server_capabilities_2 = PMA_HELPER::le_swap_u16(server_capabilities_2);
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Server capabilities 2: %#hx\n",
               server_capabilities_2);
#endif
  idx += 2;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Plugin auth.
  uint8_t plugin_data_length = 0;
  if (server_capabilities_2 & 0x8) {
    plugin_data_length = pkt_data[idx];
    idx += 1;
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: plugin_data_length: %hhu\n",
                 plugin_data_length);
#endif
  } else {
    idx += 1;
  }
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Filler
  idx += 6;
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // CLIENT_MYSQL or server_capabilities_3
  uint32_t server_capabilities_3 = 0;
  if (server_capabilities_1 & 1) {
    // filler
    idx += 4;
  } else {
    server_capabilities_3 = *reinterpret_cast<uint32_t *>(pkt_data + idx);
    server_capabilities_3 = PMA_HELPER::le_swap_u32(server_capabilities_3);
    idx += 4;
  }
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // CLIENT_SECURE_CONNECTION
  if (server_capabilities_1 & 0x80) {
    size_t size_max = 12;
    if (static_cast<size_t>(plugin_data_length) - 9 > size_max) {
      size_max = plugin_data_length - 9;
    }
#ifndef NDEBUG
    std::fprintf(stderr,
                 "NOTICE: Writing size %zu to auth_plugin_data offset 8\n",
                 size_max);
#endif
    std::memcpy(auth_plugin_data.get() + 8, pkt_data + idx, size_max);
    idx += size_max;
    idx += 1;
  }
  if (idx >= read_ret || idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  std::string auth_plugin_name;
  if (server_capabilities_2 & 0x8) {
#ifndef NDEBUG
    std::fprintf(stderr,
                 "NOTICE: at auth_plugin_name: pkt_size %" PRIu32 ", idx %zu\n",
                 pkt_size, idx);
#endif
    auto str_size =
        static_cast<std::string::allocator_type::size_type>(pkt_size - idx);
    auth_plugin_name =
        std::string(reinterpret_cast<char *>(pkt_data + idx), str_size);
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: auth_plugin_name: %s\n",
                 auth_plugin_name.c_str());
#endif
  }

  // Response.
  PMA_HELPER::BinaryParts parts;

  // Client capabilities.
  uint8_t *cli_buf = new uint8_t[4];
  uint32_t *u32 = reinterpret_cast<uint32_t *>(cli_buf);
  *u32 = 0;
  u32[0] |= 1 | 8 | 0x20;

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
  // utf8mb4_unicode_ci
  cli_buf[0] = 0xe0;
  parts.append(1, cli_buf);

  // Reserved 19 bytes.
  cli_buf = new uint8_t[19];
  std::memset(cli_buf, 0, 19);
  parts.append(19, cli_buf);

  // Extended client capabilities.
  cli_buf = new uint8_t[4];
  std::memset(cli_buf, 0, 4);
  parts.append(4, cli_buf);

  // Username.
  cli_buf = new uint8_t[user.length() + 1];
  std::memcpy(cli_buf, user.c_str(), user.length() + 1);
  parts.append(user.length() + 1, cli_buf);

  // TODO auth

  close(fd);
  return std::nullopt;
}
