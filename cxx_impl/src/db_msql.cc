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
#include <cerrno>
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

PMA_MSQL::MSQLPacket::MSQLPacket(MSQLPacket &&other)
    : packet_length(other.packet_length), seq(other.seq), body(other.body) {
  other.body = nullptr;
}

PMA_MSQL::MSQLPacket &PMA_MSQL::MSQLPacket::operator=(
    PMA_MSQL::MSQLPacket &&other) {
  if (this->body) {
    delete[] this->body;
  }

  this->packet_length = other.packet_length;
  this->seq = other.seq;
  this->body = other.body;

  other.body = nullptr;

  return *this;
}

PMA_MSQL::MSQLConnection::MSQLConnection() : flags() { flags.set(0); }

PMA_MSQL::MSQLConnection::~MSQLConnection() {
  if (fd >= 0) {
    close(fd);
  }
}

PMA_MSQL::MSQLConnection::MSQLConnection(int fd, uint32_t connection_id)
    : flags(), fd(fd), connection_id(connection_id) {
  if (fd < 0) {
    flags.set(0);
  }
}

PMA_MSQL::MSQLConnection::MSQLConnection(MSQLConnection &&other)
    : flags(other.flags), fd(other.fd), connection_id(other.connection_id) {
  other.fd = -1;
  other.flags.set(0);
}

PMA_MSQL::MSQLConnection &PMA_MSQL::MSQLConnection::operator=(
    MSQLConnection &&other) {
  if (this->fd >= 0) {
    close(this->fd);
  }

  this->flags = other.flags;
  this->fd = other.fd;
  this->connection_id = other.connection_id;

  other.fd = -1;
  other.flags.set(0);

  return *this;
}

bool PMA_MSQL::MSQLConnection::is_valid() const {
  return fd >= 0 && !flags.test(0);
}

int PMA_MSQL::MSQLConnection::execute_stmt(const std::string &stmt) {
  if (!is_valid()) {
    return 1;
  }

  uint8_t seq = 0;

  // Setup prepare stmt packet(s).
  std::vector<MSQLPacket> pkts;
  {
    uint8_t *buf = new uint8_t[stmt.size() + 1];
    buf[0] = 0x16;
    std::memcpy(buf + 1, stmt.data(), stmt.size());

    pkts = PMA_MSQL::create_packets(buf, stmt.size() + 1, &seq);
    delete[] buf;
  }

  for (const MSQLPacket &pkt : pkts) {
    PMA_HELPER::BinaryPart part;
    {
      PMA_HELPER::BinaryParts parts;
      uint32_t u32 = pkt.packet_length;
      uint8_t *u32_8 = reinterpret_cast<uint8_t *>(&u32);

      uint8_t *buf = new uint8_t[1];
      buf[0] = u32_8[0];
      parts.append(1, buf);
      buf = new uint8_t[1];
      buf[0] = u32_8[1];
      parts.append(1, buf);
      buf = new uint8_t[1];
      buf[0] = u32_8[2];
      parts.append(1, buf);

      buf = new uint8_t[1];
      buf[0] = pkt.seq;
      parts.append(1, buf);

      buf = new uint8_t[pkt.packet_length];
      std::memcpy(buf, pkt.body, pkt.packet_length);
      parts.append(pkt.packet_length, buf);

      part = parts.combine();
    }

    size_t remaining = part.size;
    while (true) {
      ssize_t write_ret =
          write(fd, part.data + (part.size - remaining), remaining);
      if (write_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifndef NDEBUG
          std::printf(".");
#endif
          continue;
        } else {
          std::fprintf(stderr, "ERROR: execute_stmt: Failed to send stmt!\n");
          return 2;
        }
      } else if (static_cast<size_t>(write_ret) < remaining) {
        remaining -= static_cast<size_t>(write_ret);
        continue;
      } else {
        std::printf("\n");
        break;
      }
    }
  }

  // Recv response pkt to prepare.
  uint32_t stmt_id;
  uint8_t *stmt_id_bytes = reinterpret_cast<uint8_t *>(&stmt_id);
  {
    uint8_t buf[4096];
    ssize_t read_ret = 0;
    while (true) {
      read_ret = read(fd, buf, 4096);
      if (read_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifndef NDEBUG
          std::printf(".");
#endif
          continue;
        }
      } else if (read_ret == 0) {
        std::fprintf(stderr,
                     "ERROR: execute_stmt: Recv EOF after sending stmt!\n");
        return 2;
      } else {
        break;
      }
    }

    size_t idx = 0;

    // Parse response pkt.
    uint32_t pkt_size;
    uint8_t *ps_bytes = reinterpret_cast<uint8_t *>(&pkt_size);
    ps_bytes[0] = buf[idx++];
    ps_bytes[1] = buf[idx++];
    ps_bytes[2] = buf[idx++];
    ps_bytes[3] = 0;

    if (idx >= static_cast<size_t>(read_ret) || idx >= 4096) {
      std::fprintf(stderr, "ERROR: execute_stmt: Recv idx out of bounds!\n");
      return 2;
    }

    uint8_t seq_id = buf[idx++];
    if (seq_id != seq) {
      std::fprintf(stderr,
                   "WARNING: execute_stmt: Recv seq %#hhx, should be %#hhx!\n",
                   seq_id, seq);
    }

    if (buf[idx] == 0xFF) {
      std::fprintf(stderr,
                   "ERROR: execute_stmt: Err pkt in response to stmt!\n");
      print_error_pkt(buf + idx, pkt_size);
      return 2;
    } else if (buf[idx] != 0) {
      std::fprintf(stderr, "ERROR: execute_stmt: Not OK pkt (%#hhx)!\n",
                   buf[idx]);
      return 2;
    }
    ++idx;

    stmt_id_bytes[0] = buf[idx++];
    stmt_id_bytes[1] = buf[idx++];
    stmt_id_bytes[2] = buf[idx++];
    stmt_id_bytes[3] = buf[idx++];

#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: stmt id %" PRIu32 " (%#" PRIx32 ")\n",
                 stmt_id, stmt_id);
#endif

    if (idx >= static_cast<size_t>(read_ret) || idx >= 4096) {
      std::fprintf(stderr, "ERROR: execute_stmt: Recv idx out of bounds!\n");
      close_stmt(stmt_id);
      return 2;
    }

    uint16_t cols;
    uint8_t *cols_bytes = reinterpret_cast<uint8_t *>(&cols);
    cols_bytes[0] = buf[idx++];
    cols_bytes[1] = buf[idx++];
    if (cols != 0) {
      std::fprintf(stderr, "WARNING: Got non-zero cols %" PRIu16 "!\n", cols);
    }

    if (idx >= static_cast<size_t>(read_ret) || idx >= 4096) {
      std::fprintf(stderr, "ERROR: execute_stmt: Recv idx out of bounds!\n");
      close_stmt(stmt_id);
      return 2;
    }

    uint16_t params;
    uint8_t *params_bytes = reinterpret_cast<uint8_t *>(&params);
    params_bytes[0] = buf[idx++];
    params_bytes[1] = buf[idx++];

    if (params != 0) {
      std::fprintf(stderr, "ERROR: stmt requires %" PRIu16 " binded params!\n",
                   params);
      close_stmt(stmt_id);
      return 2;
    }

    // unused 1 byte.
    ++idx;

    if (idx >= static_cast<size_t>(read_ret) || idx >= 4096) {
      std::fprintf(stderr, "ERROR: execute_stmt: Recv idx out of bounds!\n");
      close_stmt(stmt_id);
      return 2;
    }

    uint16_t warnings;
    uint8_t *warn_bytes = reinterpret_cast<uint8_t *>(&warnings);
    warn_bytes[0] = buf[idx++];
    warn_bytes[1] = buf[idx++];

    if (warnings > 0) {
      std::fprintf(stderr, "NOTICE: %" PRIu16 " warnings!\n", warnings);
    }

    if (idx < static_cast<size_t>(read_ret)) {
      std::fprintf(stderr, "WARNING: execute_stmt: trailing bytes %zu!\n",
                   static_cast<size_t>(read_ret) - idx);
    }
  }

  // Set up execute pkt(s).
  seq = 0;
  {
    PMA_HELPER::BinaryParts parts;

    uint8_t *buf = new uint8_t[1];
    buf[0] = 0x17;
    parts.append(1, buf);

    buf = new uint8_t[4];
    buf[0] = stmt_id_bytes[0];
    buf[1] = stmt_id_bytes[1];
    buf[2] = stmt_id_bytes[2];
    buf[3] = stmt_id_bytes[3];
    parts.append(4, buf);

    buf = new uint8_t[1];
    buf[0] = 0;
    parts.append(1, buf);

    buf = new uint8_t[4];
    buf[0] = 1;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    parts.append(4, buf);

    PMA_HELPER::BinaryPart part = parts.combine();

    pkts = PMA_MSQL::create_packets(part.data, part.size, &seq);
  }

  // Send execute pkt(s).
  for (const MSQLPacket &pkt : pkts) {
    PMA_HELPER::BinaryPart part;
    {
      PMA_HELPER::BinaryParts parts;

      uint32_t size = pkt.packet_length;
      uint8_t *size_bytes = reinterpret_cast<uint8_t *>(&size);
      uint8_t *buf = new uint8_t[3];
      buf[0] = size_bytes[0];
      buf[1] = size_bytes[1];
      buf[2] = size_bytes[2];
      parts.append(3, buf);

      buf = new uint8_t[1];
      buf[0] = pkt.seq;
      parts.append(1, buf);

      buf = new uint8_t[size];
      std::memcpy(buf, pkt.body, size);
      parts.append(size, buf);

      part = parts.combine();
    }

    size_t remaining = part.size;
    while (true) {
      ssize_t write_ret =
          write(fd, part.data + (part.size - remaining), remaining);
      if (write_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          continue;
        } else {
          std::fprintf(stderr, "Failed to send execute stmt pkt (errno %d)!\n",
                       errno);
          return 2;
        }
      } else if (static_cast<size_t>(write_ret) < remaining) {
        remaining -= static_cast<size_t>(write_ret);
      } else {
        break;
      }
    }
  }

  // Response to execute stmt pkt(s).
  {
    uint8_t buf[4096];
    size_t size = 0;
    while (true) {
      ssize_t read_ret = read(fd, buf, 4096);
      if (read_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          continue;
        } else {
          std::fprintf(stderr,
                       "ERROR: Failed to recv after exec pkt (errno %d)!\n",
                       errno);
          return 2;
        }
      } else {
        size = static_cast<size_t>(read_ret);
        break;
      }
    }

    if (size == 0) {
      std::fprintf(stderr,
                   "ERROR: Recv 0 bytes after sending exec stmt pkt!\n");
      return 2;
    }

    size_t idx = 0;

    uint32_t pkt_size;
    uint8_t *pkt_size_bytes = reinterpret_cast<uint8_t *>(&pkt_size);
    pkt_size_bytes[0] = buf[idx++];
    pkt_size_bytes[1] = buf[idx++];
    pkt_size_bytes[2] = buf[idx++];
    pkt_size_bytes[3] = 0;

    if (idx >= size && idx >= 4096) {
      std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
      return 2;
    }

    uint8_t seq_id = buf[idx++];
    if (seq_id != seq) {
      std::fprintf(stderr,
                   "WARNING: execute_stmt: Recv seq %#hhx, should be %#hhx!\n",
                   seq_id, seq);
    }

    if (idx >= size && idx >= 4096) {
      std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
      return 2;
    }

    if (buf[idx] == 0xFF) {
      std::fprintf(stderr, "ERROR: Recv Err pkt after exec pkt sent!\n");
      print_error_pkt(buf + idx, pkt_size);
      return 2;
    } else if (buf[idx] == 0) {
      // OK pkt.
      ++idx;
      if (idx >= size && idx >= 4096) {
        std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
        return 2;
      }

      uint64_t rows_size;
      uint_fast8_t bytes_read;
      std::tie(rows_size, bytes_read) = parse_len_enc_int(buf + idx);
      idx += bytes_read;
#ifndef NDEBUG
      std::fprintf(stderr, "stmt affected %" PRIu64 " rows!\n", rows_size);
#endif
      if (idx >= size && idx >= 4096) {
        std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
        return 2;
      }

      uint64_t last_insert_id;
      std::tie(last_insert_id, bytes_read) = parse_len_enc_int(buf + idx);
      idx += bytes_read;
#ifndef NDEBUG
      std::fprintf(stderr, "last insert id: %" PRIu64 "\n", last_insert_id);
#endif
      if (idx >= size && idx >= 4096) {
        std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
        return 2;
      }

      uint16_t status;
      uint8_t *status_bytes = reinterpret_cast<uint8_t *>(&status);
      status_bytes[0] = buf[idx++];
      status_bytes[1] = buf[idx++];
#ifndef NDEBUG
      std::fprintf(stderr, "Server status: %#" PRIx16 "\n", status);
#endif
      if (idx >= size && idx >= 4096) {
        std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
        return 2;
      }

      uint16_t warning_c;
      uint8_t *warning_c_bytes = reinterpret_cast<uint8_t *>(&warning_c);
      warning_c_bytes[0] = buf[idx++];
      warning_c_bytes[1] = buf[idx++];
#ifndef NDEBUG
      std::fprintf(stderr, "Warning count: %" PRIu16 "\n", warning_c);
#endif
      if (idx == size) {
        close_stmt(stmt_id);
        return 0;
      }

      if (idx >= size && idx >= 4096) {
        std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
        return 2;
      }

      uint64_t info_string_size;
      std::tie(info_string_size, bytes_read) = parse_len_enc_int(buf + idx);
      idx += bytes_read;

      if (idx + info_string_size > size || idx + info_string_size >= 4096) {
        std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
        return 2;
      }

      std::fprintf(stderr, "%.*s", static_cast<int>(info_string_size),
                   buf + idx);
    } else {
      // TODO handle result set pkts.
    }
  }
  close_stmt(stmt_id);
  return 0;
}

void PMA_MSQL::MSQLConnection::close_stmt(uint32_t stmt_id) {
  if (!is_valid()) {
    return;
  }

  uint8_t buf[9];
  buf[0] = 5;
  buf[1] = 0;
  buf[2] = 0;
  buf[3] = 0;
  buf[4] = 0x19;

  uint8_t *id_bytes = reinterpret_cast<uint8_t *>(&stmt_id);

  buf[5] = id_bytes[0];
  buf[6] = id_bytes[1];
  buf[7] = id_bytes[2];
  buf[8] = id_bytes[3];

  ssize_t write_ret;
  while (true) {
    write_ret = write(fd, buf, 9);
    if (write_ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else {
        std::fprintf(stderr, "ERROR: Failed to send close stmt packet!\n");
        return;
      }
    } else {
      if (write_ret != 9) {
        std::fprintf(stderr, "WARNING: Sent %zd of 9 bytes!\n", write_ret);
      }
      break;
    }
  }
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

std::optional<PMA_MSQL::MSQLConnection> PMA_MSQL::connect_msql(
    std::string addr, uint16_t port, std::string user, std::string pass,
    std::string dbname) {
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
  size_t auth_plugin_data_size = 8;
  if (server_capabilities_1 & 0x80) {
    size_t size_max = 12;
    if (static_cast<size_t>(plugin_data_length) - 9 > size_max) {
      size_max = plugin_data_length - 9;
    }
    auth_plugin_data_size += size_max;
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
  *u32 |= 1 | 8 | (1 << 9) | (1 << 15) | (1 << 19) | (1 << 21) | (1 << 24);

  *u32 = PMA_HELPER::le_swap_u32(*u32);

  parts.append(4, cli_buf);

  // Max packet size.
  cli_buf = new uint8_t[4];
  u32 = reinterpret_cast<uint32_t *>(cli_buf);
  *u32 = 0x1000;

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

  // Pass native auth.
  if (auth_plugin_data_size != 20) {
    std::fprintf(stderr, "ERROR: seed from server is not 20 bytes!\n");
    close(fd);
    return std::nullopt;
  }
  std::vector<uint8_t> seed_vec;
  for (size_t sidx = 0; sidx < auth_plugin_data_size; ++sidx) {
    seed_vec.push_back(auth_plugin_data[sidx]);
  }

  std::array<uint8_t, 20> auth_arr = msql_native_auth_resp(seed_vec, pass);
  if (server_capabilities_2 & (1 << (21 - 16))) {
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: LENENC auth response.\n");
#endif
    cli_buf = new uint8_t[1];
    cli_buf[0] = 20;
    parts.append(1, cli_buf);

    cli_buf = new uint8_t[auth_arr.size()];
    std::memcpy(cli_buf, auth_arr.data(), auth_arr.size());
    parts.append(auth_arr.size(), cli_buf);
  } else if (server_capabilities_1 & (1 << 15)) {
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: 1 byte size auth response.\n");
#endif
    cli_buf = new uint8_t[1 + auth_arr.size()];
    cli_buf[0] = auth_arr.size();
    std::memcpy(cli_buf + 1, auth_arr.data(), auth_arr.size());
    parts.append(1 + auth_arr.size(), cli_buf);
  } else {
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: Null terminated auth response.\n");
#endif
    cli_buf = new uint8_t[1 + auth_arr.size()];
    std::memcpy(cli_buf, auth_arr.data(), auth_arr.size());
    cli_buf[auth_arr.size()] = 0;
    parts.append(1 + auth_arr.size(), cli_buf);
  }

  if (server_capabilities_1 & 8) {
    cli_buf = new uint8_t[1 + dbname.size()];
    std::memcpy(cli_buf, dbname.c_str(), dbname.size() + 1);
    parts.append(1 + dbname.size(), cli_buf);
  }

  cli_buf = new uint8_t[auth_plugin_name.size() + 1];
  std::memcpy(cli_buf, auth_plugin_name.c_str(), auth_plugin_name.size() + 1);
  parts.append(auth_plugin_name.size() + 1, cli_buf);

  cli_buf = new uint8_t[1];
  cli_buf[0] = 0;
  parts.append(1, cli_buf);

  // Send combined data.
  const PMA_HELPER::BinaryPart combined = parts.combine();
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Client send size: %zu\n", combined.size);
#endif
  sequence_id += 1;
  std::vector<MSQLPacket> write_pkts =
      create_packets(combined.data, combined.size, &sequence_id);
  ssize_t write_ret = 0;
  for (size_t pkts_idx = 0; pkts_idx < write_pkts.size(); ++pkts_idx) {
    PMA_HELPER::BinaryParts pkt_parts;

    cli_buf = new uint8_t[3];
    uint32_t packet_length = write_pkts[pkts_idx].packet_length;
    uint8_t *ptr = reinterpret_cast<uint8_t *>(&packet_length);
    cli_buf[0] = ptr[0];
    cli_buf[1] = ptr[1];
    cli_buf[2] = ptr[2];
    pkt_parts.append(3, cli_buf);

    cli_buf = new uint8_t[1];
    cli_buf[0] = write_pkts[pkts_idx].seq;
    pkt_parts.append(1, cli_buf);

    cli_buf = new uint8_t[write_pkts[pkts_idx].packet_length];
    std::memcpy(cli_buf, write_pkts[pkts_idx].body,
                write_pkts[pkts_idx].packet_length);
    pkt_parts.append(write_pkts[pkts_idx].packet_length, cli_buf);

    const PMA_HELPER::BinaryPart pkt_data = pkt_parts.combine();

    size_t remaining = pkt_data.size;
    while (true) {
      write_ret =
          write(fd, pkt_data.data + (pkt_data.size - remaining), remaining);
      if (write_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // NONBLOCKING nothing to read.
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifndef NDEBUG
          std::printf(".");
#endif
        } else {
          std::fprintf(
              stderr, "ERROR: Error occurred writing data (errno %d)\n", errno);
          return std::nullopt;
        }
      } else if (write_ret < static_cast<ssize_t>(remaining)) {
        remaining -= static_cast<size_t>(write_ret);
      } else {
        break;
      }
    }
    std::printf("\n");
  }

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
      std::fprintf(stderr, "ERROR: Failed to handshake with msql server!\n");
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

  pkt_size = static_cast<uint32_t>(buf[0]) |
             (static_cast<uint32_t>(buf[1]) << 8) |
             (static_cast<uint32_t>(buf[2]) << 16);
#ifndef NDEBUG
  std::fprintf(stderr, "pkt_size: %" PRIu32 " (%x)\n", pkt_size, pkt_size);
#endif
  sequence_id = buf[3];
#ifndef NDEBUG
  std::fprintf(stderr, "seq: %hhu\n", sequence_id);
#endif

  pkt_data = buf + 4;
  idx = 0;

  // "Header" byte.
  if (server_capabilities_2 & (1 << (24 - 16))) {
    // CLIENT_DEPRECATE_EOF set.
    if (pkt_data[idx] == 0) {
#ifndef NDEBUG
      std::fprintf(stderr, "NOTICE: Got 0 as OK from server.\n");
#endif
    } else {
      std::fprintf(stderr, "ERROR: Got invalid %#hhx from server (not 0xFE)!\n",
                   pkt_data[idx]);
      close(fd);
      print_error_pkt(pkt_data, pkt_size);
      return std::nullopt;
    }
  } else {
    if (pkt_data[idx] == 0) {
#ifndef NDEBUG
      std::fprintf(stderr, "NOTICE: Got 0 as OK from server.\n");
#endif
    } else {
      std::fprintf(stderr, "ERROR: Got invalid %#hhx from server (not 0)!\n",
                   pkt_data[idx]);
      close(fd);
      print_error_pkt(pkt_data, pkt_size);
      return std::nullopt;
    }
  }
  ++idx;

  // Affected rows.
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Affected rows: %hhu (%#hhx)\n", pkt_data[idx],
               pkt_data[idx]);
#endif
  ++idx;

  // Last insert id.
  if (pkt_data[idx] < 0xFB) {
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: Last insert id: %#hhx\n", pkt_data[idx]);
#endif
    ++idx;
  } else if (pkt_data[idx] == 0xFC) {
    ++idx;
    uint16_t u16;
    uint8_t *u16_8 = reinterpret_cast<uint8_t *>(&u16);
    u16_8[0] = pkt_data[idx++];
    u16_8[1] = pkt_data[idx++];

#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: Last insert id: %" PRIu16 " (%#" PRIx16 ")\n",
                 u16, u16);
#endif
  } else if (pkt_data[idx] == 0xFD) {
    ++idx;
    uint32_t u32;
    uint8_t *u32_8 = reinterpret_cast<uint8_t *>(&u32);
    u32_8[0] = pkt_data[idx++];
    u32_8[1] = pkt_data[idx++];
    u32_8[2] = pkt_data[idx++];
    u32_8[3] = 0;
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: Last insert id: %" PRIu32 " (%#" PRIx32 ")\n",
                 u32, u32);
#endif
  } else if (pkt_data[idx] == 0xFE) {
    ++idx;
    uint64_t u64;
    uint8_t *u64_8 = reinterpret_cast<uint8_t *>(&u64);
    u64_8[0] = pkt_data[idx++];
    u64_8[1] = pkt_data[idx++];
    u64_8[2] = pkt_data[idx++];
    u64_8[3] = pkt_data[idx++];
    u64_8[4] = pkt_data[idx++];
    u64_8[5] = pkt_data[idx++];
    u64_8[6] = pkt_data[idx++];
    u64_8[7] = pkt_data[idx++];
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: Last insert id: %" PRIu64 " (%#" PRIx64 ")\n",
                 u64, u64);
#endif
  } else if (pkt_data[idx] == 0xFB) {
    ++idx;
    std::fprintf(stderr, "NOTICE: Last insert id is NULL\n");
  } else {
    ++idx;
    std::fprintf(stderr, "WARNING: Got 0xFF for last insert id!\n");
  }

  if (idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server status 16 bits.
  uint16_t server_status;
  uint8_t *server_status_bytes = reinterpret_cast<uint8_t *>(&server_status);
  server_status_bytes[0] = pkt_data[idx++];
  server_status_bytes[1] = pkt_data[idx++];

#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Server status: %#" PRIx16 "\n", server_status);
#endif

  if (idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Warning count.
  uint16_t warning_count;
  uint8_t *warning_count_bytes = reinterpret_cast<uint8_t *>(&warning_count);
  warning_count_bytes[0] = pkt_data[idx++];
  warning_count_bytes[1] = pkt_data[idx++];

  std::fprintf(stderr, "NOTICE: Server warning count: %" PRIu16 "\n",
               warning_count);

  if (idx >= 4092 || idx >= pkt_size) {
    close(fd);
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  return MSQLConnection(fd, connection_id);
}

std::array<uint8_t, 20> PMA_MSQL::msql_native_auth_resp(
    std::vector<uint8_t> seed, std::string pass) {
  std::array<uint8_t, 20> pass_sha1 = PMA_HELPER::sha1_digest(
      reinterpret_cast<uint8_t *>(pass.data()), pass.size());

  std::array<uint8_t, 20> pass_sha1_sha1 =
      PMA_HELPER::sha1_digest(pass_sha1.data(), pass_sha1.size());

  std::vector<uint8_t> seed_and_pass_concat;
  for (size_t idx = 0; idx < seed.size(); ++idx) {
    seed_and_pass_concat.push_back(seed.at(idx));
  }
  for (size_t idx = 0; idx < 20; ++idx) {
    seed_and_pass_concat.push_back(pass_sha1_sha1.at(idx));
  }

  std::array<uint8_t, 20> seed_and_pass_xor_sha1 = PMA_HELPER::sha1_digest(
      seed_and_pass_concat.data(), seed_and_pass_concat.size());

  std::array<uint8_t, 20> ret;
  for (size_t idx = 0; idx < 20; ++idx) {
    ret.at(idx) = pass_sha1.at(idx) ^ seed_and_pass_xor_sha1.at(idx);
  }

  return ret;
}

void PMA_MSQL::print_error_pkt(uint8_t *data, size_t size) {
  size_t idx = 0;

  if (data[idx] != 0xFF) {
    std::fprintf(stderr, "ERROR: First byte of error packet isn't 0xFF!\n");
    return;
  }
  ++idx;

  if (idx >= size) {
    std::fprintf(stderr, "End of error pkt.\n");
    return;
  }

  // error code.
  uint16_t u16;
  uint8_t *u16_8 = reinterpret_cast<uint8_t *>(&u16);
  u16_8[0] = data[idx++];
  u16_8[1] = data[idx++];

  std::fprintf(stderr, "Error code %" PRIu16 " (%#" PRIx16 ")\n", u16, u16);

  if (idx >= size) {
    std::fprintf(stderr, "End of error pkt.\n");
    return;
  }

  if (u16 == 0xFFFF) {
    uint8_t stage = data[idx++];
    uint8_t max_stage = data[idx++];
    std::fprintf(stderr, "Stage %" PRIu8 " of %" PRIu8 "\n", stage, max_stage);
    if (idx >= size) {
      std::fprintf(stderr, "End of error pkt.\n");
      return;
    }
    uint32_t u32;
    uint8_t *u32_8 = reinterpret_cast<uint8_t *>(&u32);
    u32_8[0] = data[idx++];
    u32_8[1] = data[idx++];
    u32_8[2] = data[idx++];
    u32_8[3] = 0;
    std::fprintf(stderr, "Progress: %" PRIu32 " (%#" PRIx32 ")\n", u32, u32);
    if (idx >= size) {
      std::fprintf(stderr, "End of error pkt.\n");
      return;
    }

    size_t str_len = 0;
    if (data[idx] < 0xFB) {
      str_len = data[idx + 1];
      idx += 2;
      std::string progress(reinterpret_cast<char *>(data + idx), str_len);
      std::fprintf(stderr, "Progress String: %s\n", progress.c_str());
    } else if (data[idx] == 0xFB) {
      std::fprintf(stderr, "NULL progress info\n");
    } else if (data[idx] == 0xFC) {
      ++idx;
      u16_8[0] = data[idx++];
      u16_8[1] = data[idx++];
      std::string progress(reinterpret_cast<char *>(data + idx), u16);
      std::fprintf(stderr, "Progress String: %s\n", progress.c_str());
    } else if (data[idx] == 0xFD) {
      ++idx;
      u32_8[0] = data[idx++];
      u32_8[1] = data[idx++];
      u32_8[2] = data[idx++];
      u32_8[3] = 0;
      std::string progress(reinterpret_cast<char *>(data + idx), u32);
      std::fprintf(stderr, "Progress String: %s\n", progress.c_str());
    } else if (data[idx] == 0xFE) {
      ++idx;
      uint64_t u64;
      uint8_t *u64_8 = reinterpret_cast<uint8_t *>(&u64);
      u64_8[0] = data[idx++];
      u64_8[1] = data[idx++];
      u64_8[2] = data[idx++];
      u64_8[3] = data[idx++];
      u64_8[4] = data[idx++];
      u64_8[5] = data[idx++];
      u64_8[6] = data[idx++];
      u64_8[7] = data[idx++];
      std::string progress(reinterpret_cast<char *>(data + idx), u64);
      std::fprintf(stderr, "Progress String: %s\n", progress.c_str());
    } else {
      std::fprintf(stderr, "Got 0xFF processing progress string.\n");
      return;
    }
    return;
  }

  if (data[idx] == '#') {
    ++idx;
    std::fprintf(stderr, "SQL state: %.5s\n", data + idx);
    idx += 5;
    if (idx < size) {
      std::fprintf(stderr, "%.*s\n", static_cast<int>(size - idx), data + idx);
    }
  } else {
    if (idx < size) {
      std::fprintf(stderr, "%.*s\n", static_cast<int>(size - idx), data + idx);
    }
  }
}

std::tuple<uint64_t, uint_fast8_t> PMA_MSQL::parse_len_enc_int(uint8_t *data) {
  uint64_t i;
  uint8_t *i_bytes = reinterpret_cast<uint8_t *>(&i);

  uint_fast8_t idx = 0;
  if (data[idx] < 0xFB) {
    i = data[idx];
    return {i, 1};
  } else if (data[idx] == 0xFB) {
    return {0, 1};
  } else if (data[idx] == 0xFC) {
    ++idx;
    i_bytes[0] = data[idx++];
    i_bytes[1] = data[idx++];
    i_bytes[2] = 0;
    i_bytes[3] = 0;
    i_bytes[4] = 0;
    i_bytes[5] = 0;
    i_bytes[6] = 0;
    i_bytes[7] = 0;
    return {i, idx};
  } else if (data[idx] == 0xFD) {
    ++idx;
    i_bytes[0] = data[idx++];
    i_bytes[1] = data[idx++];
    i_bytes[2] = data[idx++];
    i_bytes[3] = 0;
    i_bytes[4] = 0;
    i_bytes[5] = 0;
    i_bytes[6] = 0;
    i_bytes[7] = 0;
    return {i, idx};
  } else if (data[idx] == 0xFE) {
    ++idx;
    i_bytes[0] = data[idx++];
    i_bytes[1] = data[idx++];
    i_bytes[2] = data[idx++];
    i_bytes[3] = data[idx++];
    i_bytes[4] = data[idx++];
    i_bytes[5] = data[idx++];
    i_bytes[6] = data[idx++];
    i_bytes[7] = data[idx++];
    return {i, idx};
  }

  return {0, 0};
}
