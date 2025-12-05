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
#include "db.h"
#include "helpers.h"
#include "http.h"
#include "poor_mans_print.h"

static const char *DB_INIT_TABLE_SEQ_ID =
    "CREATE TABLE IF NOT EXISTS CXX_SEQ_ID ("
    "  ID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
    "  SEQ_ID INT UNSIGNED NOT NULL"
    ")";

static const char *DB_INIT_TABLE_CHALLENGE_FACTORS =
    "CREATE TABLE IF NOT EXISTS CXX_CHALLENGE_FACTORS ("
    "  ID CHAR(64) CHARACTER SET ascii NOT NULL PRIMARY KEY,"
    "  IP VARCHAR(45) NOT NULL,"
    "  FACTORS CHAR(64) CHARACTER SET ascii NOT NULL,"
    "  PORT INT UNSIGNED NOT NULL,"
    "  GEN_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "  INDEX ON_TIME_INDEX USING BTREE (GEN_TIME)"
    ")";

static const char *DB_INIT_TABLE_ALLOWED_IPS =
    "CREATE TABLE IF NOT EXISTS CXX_ALLOWED_IPS ("
    "  ID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
    "  IP VARCHAR(45) NOT NULL,"
    "  PORT INT UNSIGNED NOT NULL,"
    "  ON_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "  INDEX IP_PORT_INDEX USING HASH (IP, PORT),"
    "  INDEX ON_TIME_INDEX USING BTREE (ON_TIME)"
    ")";

static const char *DB_INIT_TABLE_ID_TO_PORT =
    "CREATE TABLE IF NOT EXISTS CXX_ID_TO_PORT ("
    "  ID CHAR(64) CHARACTER SET ascii NOT NULL PRIMARY KEY,"
    "  PORT INT UNSIGNED NOT NULL,"
    "  ON_TIME DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,"
    "  INDEX ON_TIME_INDEX USING BTREE (ON_TIME)"
    ")";

static const char *DB_GET_SEQ_ID = "SELECT ID, SEQ_ID FROM CXX_SEQ_ID";
static const char *DB_REMOVE_SEQ_ID = "DELETE FROM CXX_SEQ_ID WHERE ID = ?";
static const char *DB_ADD_SEQ_ID = "INSERT INTO CXX_SEQ_ID (SEQ_ID) VALUES (?)";
static const char *DB_UPDATE_SEQ_ID = "UPDATE CXX_SEQ_ID SET SEQ_ID = ?";

static const char *DB_SEL_CHAL_FACT_BY_ID =
    "SELECT ID FROM CXX_CHALLENGE_FACTORS WHERE ID = ?";
static const char *DB_ADD_CHAL_FACT =
    "INSERT INTO CXX_CHALLENGE_FACTORS (ID, IP, PORT, FACTORS) VALUES (?, ?, "
    "?, ?)";

static const char *DB_GET_PORT_ID_TO_PORT =
    "SELECT PORT FROM CXX_ID_TO_PORT WHERE ID = ?";
static const char *DB_DEL_ID_TO_PORT_ENTRY =
    "DELETE FROM CXX_ID_TO_PORT WHERE ID = ?";

static const char *DB_IP_PORT_FROM_CHAL_FACT =
    "SELECT IP, PORT FROM CXX_CHALLENGE_FACTORS WHERE ID = ? AND FACTORS = ?";
static const char *DB_DEL_FROM_CHAL_FACT =
    "DELETE FROM CXX_CHALLENGE_FACTORS WHERE ID = ?";
static const char *DB_ADD_ALLOWED_IPS_ENTRY =
    "INSERT INTO CXX_ALLOWED_IPS (IP, PORT) VALUES (?, ?)";

static const char *DB_IS_ALLOWED_IPS =
    "SELECT IP, ON_TIME FROM CXX_ALLOWED_IPS WHERE IP = ? AND PORT = ?";

static const char *DB_ADD_ID_TO_PORT =
    "INSERT INTO CXX_ID_TO_PORT (ID, PORT) VALUES (?, ?)";

static const char *DB_CLEANUP_CHAL_FACT =
    "DELETE FROM CXX_CHALLENGE_FACTORS WHERE TIMESTAMPDIFF(MINUTE, GEN_TIME, "
    "NOW()) >= ?";
static const char *DB_CLEANUP_ALLOWED_IPS =
    "DELETE FROM CXX_ALLOWED_IPS WHERE TIMESTAMPDIFF(MINUTE, ON_TIME, NOW()) "
    ">= ?";
static const char *DB_CLEANUP_ID_TO_PORT =
    "DELETE FROM CXX_ID_TO_PORT WHERE TIMESTAMPDIFF(MINUTE, ON_TIME, NOW()) >= "
    "?";

static std::chrono::milliseconds CONN_TRY_LOCK_DURATION =
    std::chrono::milliseconds(500);

PMA_MSQL::Packet::Packet() : packet_length(0), seq(0), body(nullptr) {}

PMA_MSQL::Packet::~Packet() {
  if (body) {
    delete[] body;
  }
}

PMA_MSQL::Packet::Packet(uint32_t len, uint8_t seq, uint8_t *data)
    : packet_length(len & 0xFFFFFF), seq(seq), body(data) {}

PMA_MSQL::Packet::Packet(Packet &&other)
    : packet_length(other.packet_length), seq(other.seq), body(other.body) {
  other.body = nullptr;
}

PMA_MSQL::Packet &PMA_MSQL::Packet::operator=(PMA_MSQL::Packet &&other) {
  if (this->body) {
    delete[] this->body;
  }

  this->packet_length = other.packet_length;
  this->seq = other.seq;
  this->body = other.body;

  other.body = nullptr;

  return *this;
}

PMA_MSQL::Value::Value() : u(), type_enum(PMA_MSQL::Value::INV_NULL) {}
PMA_MSQL::Value::Value(std::string str) : u(str), type_enum(STRING) {}
PMA_MSQL::Value::Value(int64_t i) : u(i), type_enum(SIGNED_INT) {}
PMA_MSQL::Value::Value(uint64_t u) : u(u), type_enum(UNSIGNED_INT) {}
PMA_MSQL::Value::Value(double d) : u(d), type_enum(DOUBLE) {}

PMA_MSQL::Value::~Value() {
  switch (type_enum) {
    case SIGNED_INT:
      this->u.sint.~shared_ptr<int64_t>();
      break;
    case UNSIGNED_INT:
      this->u.uint.~shared_ptr<uint64_t>();
      break;
    case DOUBLE:
      this->u.d.~shared_ptr<double>();
      break;
    case STRING:
      this->u.str.~shared_ptr<std::string>();
      break;
    case INV_NULL:
      break;
  }
}

PMA_MSQL::Value::Value(const Value &other) : u(), type_enum(other.type_enum) {
  switch (type_enum) {
    case SIGNED_INT:
      new (&this->u.sint) std::shared_ptr<int64_t>();
      this->u.sint = std::make_shared<int64_t>(*other.u.sint.get());
      break;
    case UNSIGNED_INT:
      new (&this->u.uint) std::shared_ptr<uint64_t>();
      this->u.uint = std::make_shared<uint64_t>(*other.u.uint.get());
      break;
    case DOUBLE:
      new (&this->u.d) std::shared_ptr<double>();
      this->u.d = std::make_shared<double>(*other.u.d.get());
      break;
    case STRING:
      new (&this->u.str) std::shared_ptr<std::string>();
      this->u.str = std::make_shared<std::string>(*other.u.str.get());
      break;
    case INV_NULL:
      break;
  }
}

PMA_MSQL::Value &PMA_MSQL::Value::operator=(const Value &other) {
  this->~Value();

  this->type_enum = other.type_enum;

  switch (this->type_enum) {
    case SIGNED_INT:
      new (&this->u.sint) std::shared_ptr<int64_t>();
      this->u.sint = std::make_shared<int64_t>(*other.u.sint.get());
      break;
    case UNSIGNED_INT:
      new (&this->u.uint) std::shared_ptr<uint64_t>();
      this->u.uint = std::make_shared<uint64_t>(*other.u.uint.get());
      break;
    case DOUBLE:
      new (&this->u.d) std::shared_ptr<double>();
      this->u.d = std::make_shared<double>(*other.u.d.get());
      break;
    case STRING:
      new (&this->u.str) std::shared_ptr<std::string>();
      this->u.str = std::make_shared<std::string>(*other.u.str.get());
      break;
    case INV_NULL:
      break;
  }

  return *this;
}

PMA_MSQL::Value::Value(Value &&other) : u(), type_enum(other.type_enum) {
  switch (type_enum) {
    case SIGNED_INT:
      new (&this->u.sint) std::shared_ptr<int64_t>();
      this->u.sint = std::move(other.u.sint);
      break;
    case UNSIGNED_INT:
      new (&this->u.uint) std::shared_ptr<uint64_t>();
      this->u.uint = std::move(other.u.uint);
      break;
    case DOUBLE:
      new (&this->u.d) std::shared_ptr<double>();
      this->u.d = std::move(other.u.d);
      break;
    case STRING:
      new (&this->u.str) std::shared_ptr<std::string>();
      this->u.str = std::move(other.u.str);
      break;
    case INV_NULL:
      break;
  }
}

PMA_MSQL::Value &PMA_MSQL::Value::operator=(Value &&other) {
  this->~Value();

  this->type_enum = other.type_enum;

  switch (this->type_enum) {
    case SIGNED_INT:
      new (&this->u.sint) std::shared_ptr<int64_t>();
      this->u.sint = std::move(other.u.sint);
      break;
    case UNSIGNED_INT:
      new (&this->u.uint) std::shared_ptr<uint64_t>();
      this->u.uint = std::move(other.u.uint);
      break;
    case DOUBLE:
      new (&this->u.d) std::shared_ptr<double>();
      this->u.d = std::move(other.u.d);
      break;
    case STRING:
      new (&this->u.str) std::shared_ptr<std::string>();
      this->u.str = std::move(other.u.str);
      break;
    case INV_NULL:
      break;
  }

  other.~Value();
  other.type_enum = INV_NULL;

  return *this;
}

PMA_MSQL::Value PMA_MSQL::Value::new_int(int64_t i) { return Value(i); }

PMA_MSQL::Value PMA_MSQL::Value::new_uint(uint64_t u) { return Value(u); }

PMA_MSQL::Value::TypeE PMA_MSQL::Value::get_type() const { return type_enum; }

std::optional<std::shared_ptr<std::string> > PMA_MSQL::Value::get_str() {
  if (type_enum == STRING) {
    return u.str;
  } else {
    return std::nullopt;
  }
}

std::optional<std::shared_ptr<int64_t> > PMA_MSQL::Value::get_signed_int() {
  if (type_enum == SIGNED_INT) {
    return u.sint;
  } else {
    return std::nullopt;
  }
}

std::optional<std::shared_ptr<uint64_t> > PMA_MSQL::Value::get_unsigned_int() {
  if (type_enum == UNSIGNED_INT) {
    return u.uint;
  } else {
    return std::nullopt;
  }
}

std::optional<std::shared_ptr<double> > PMA_MSQL::Value::get_double() {
  if (type_enum == DOUBLE) {
    return u.d;
  } else {
    return std::nullopt;
  }
}

std::optional<std::shared_ptr<const std::string> > PMA_MSQL::Value::get_str()
    const {
  if (type_enum == STRING) {
    return u.str;
  } else {
    return std::nullopt;
  }
}

std::optional<std::shared_ptr<const int64_t> > PMA_MSQL::Value::get_signed_int()
    const {
  if (type_enum == SIGNED_INT) {
    return u.sint;
  } else {
    return std::nullopt;
  }
}

std::optional<std::shared_ptr<const uint64_t> >
PMA_MSQL::Value::get_unsigned_int() const {
  if (type_enum == UNSIGNED_INT) {
    return u.uint;
  } else {
    return std::nullopt;
  }
}

std::optional<std::shared_ptr<const double> > PMA_MSQL::Value::get_double()
    const {
  if (type_enum == DOUBLE) {
    return u.d;
  } else {
    return std::nullopt;
  }
}

PMA_MSQL::Value::U::U() {}
PMA_MSQL::Value::U::~U() {}
PMA_MSQL::Value::U::U(std::string s) {
  new (&this->str) std::shared_ptr<std::string>();
  this->str = std::make_shared<std::string>(std::forward<std::string>(s));
}
PMA_MSQL::Value::U::U(int64_t i) {
  new (&this->sint) std::shared_ptr<int64_t>();
  this->sint = std::make_shared<int64_t>(i);
}
PMA_MSQL::Value::U::U(uint64_t u) {
  new (&this->uint) std::shared_ptr<uint64_t>();
  this->uint = std::make_shared<uint64_t>(u);
}
PMA_MSQL::Value::U::U(double d) {
  new (&this->d) std::shared_ptr<double>();
  this->d = std::make_shared<double>(d);
}

std::timed_mutex PMA_MSQL::Connection::m = std::timed_mutex();

PMA_MSQL::Connection::Connection() : flags(), fd(-1) { flags.set(0); }

PMA_MSQL::Connection::~Connection() {
  if (!flags.test(0)) {
    if (fd >= 0) {
      std::vector<uint8_t> quit_pkt;
      quit_pkt.push_back(1);
      quit_pkt.push_back(0);
      quit_pkt.push_back(0);
      quit_pkt.push_back(0);
      quit_pkt.push_back(1);
      uint_fast8_t ticks = 0;
      size_t remaining = 5;
      while (remaining != 0) {
        ssize_t write_ret =
            write(fd, quit_pkt.data() + (5 - remaining), remaining);
        if (write_ret == -1) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            if (ticks++ > 200) {
              break;
            }
            continue;
          } else {
            break;
          }
        } else if (write_ret > 0 &&
                   static_cast<size_t>(write_ret) <= remaining) {
          remaining -= static_cast<size_t>(write_ret);
          continue;
        } else {
          break;
        }
      }
      close(fd);
    }
    m.unlock();
  }
}

PMA_MSQL::Connection::Connection(int fd, uint32_t connection_id)
    : flags(), fd(fd), connection_id(connection_id) {
  if (fd < 0) {
    flags.set(0);
  }
}

PMA_MSQL::Connection::Connection(Connection &&other)
    : flags(other.flags), fd(other.fd), connection_id(other.connection_id) {
  other.fd = -1;
  other.flags.set(0);
}

PMA_MSQL::Connection &PMA_MSQL::Connection::operator=(Connection &&other) {
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

bool PMA_MSQL::Connection::is_valid() const {
  return fd >= 0 && !flags.test(0);
}

PMA_MSQL::Connection::StmtRet PMA_MSQL::Connection::execute_stmt(
    const std::string &stmt, std::vector<Value> bind_params) {
  if (!is_valid()) {
    return std::nullopt;
  }

  uint8_t seq = 0;

  // Setup prepare stmt packet(s).
  std::vector<Packet> pkts;
  {
    uint8_t *buf = new uint8_t[stmt.size() + 1];
    buf[0] = 0x16;
    std::memcpy(buf + 1, stmt.data(), stmt.size());

    pkts = PMA_MSQL::create_packets(buf, stmt.size() + 1, &seq);
    delete[] buf;
  }

  for (const Packet &pkt : pkts) {
    PMA_HELPER::BinaryPart part;
    {
      PMA_HELPER::BinaryParts parts;
      uint32_t u32 = pkt.packet_length;
      uint8_t *u32_8 = reinterpret_cast<uint8_t *>(&u32);

      uint8_t *buf = new uint8_t[3];
      buf[0] = u32_8[0];
      buf[1] = u32_8[1];
      buf[2] = u32_8[2];
      parts.append(3, buf);

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
          std::fprintf(stderr, "w");
#endif
          continue;
        } else {
          std::fprintf(stderr, "ERROR: execute_stmt: Failed to send stmt!\n");
          return std::nullopt;
        }
      } else if (static_cast<size_t>(write_ret) < remaining) {
        remaining -= static_cast<size_t>(write_ret);
        continue;
      } else {
#ifndef NDEBUG
        std::fprintf(stderr, "\n");
#endif
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
          std::fprintf(stderr, "r");
#endif
          continue;
        }
      } else if (read_ret == 0) {
        std::fprintf(stderr,
                     "ERROR: execute_stmt: Recv EOF after sending stmt!\n");
        return std::nullopt;
      } else {
#ifndef NDEBUG
        std::fprintf(stderr, "\n");
#endif
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
      return std::nullopt;
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
      return std::nullopt;
    } else if (buf[idx] != 0) {
      std::fprintf(stderr, "ERROR: execute_stmt: Not OK pkt (%#hhx)!\n",
                   buf[idx]);
      return std::nullopt;
    }

    auto res_opt = parse_prepare_resp_pkt(buf + idx, pkt_size);
    if (!res_opt.has_value()) {
      PMA_EPrintln("ERROR: Failed to parse prepare response pkt!");
      return std::nullopt;
    }
    int ret;
    std::tie(ret, stmt_id) = std::move(res_opt.value());
    if (ret != 0) {
      PMA_EPrintln("ERROR: Failed to parse prepare response pkt!");
      close_stmt(stmt_id);
      return std::nullopt;
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

    // Params.
    if (!bind_params.empty()) {
      // NULL bitmap.
      const size_t bitmap_size = (bind_params.size() + 7) / 8;
      uint8_t *bitmap_ptr = new uint8_t[bitmap_size];
      std::memset(bitmap_ptr, 0, bitmap_size);
      for (size_t pidx = 0; pidx < bind_params.size(); ++pidx) {
        if (bind_params.at(pidx).get_type() == Value::INV_NULL) {
          size_t bitmap_byte = 0;
          size_t offset = pidx;
          while (offset > 7) {
            ++bitmap_byte;
            offset -= 8;
          }
          bitmap_ptr[bitmap_byte] |= 1 << offset;
        }
      }
      parts.append(bitmap_size, bitmap_ptr);

      // Per param type enabled.
      buf = new uint8_t[1];
      buf[0] = 1;
      parts.append(1, buf);

      // Per param type.
      for (size_t pidx = 0; pidx < bind_params.size(); ++pidx) {
        switch (bind_params.at(pidx).get_type()) {
          case Value::INV_NULL:
            buf = new uint8_t[2];
            buf[0] = 6;
            buf[1] = 0;
            parts.append(2, buf);
            break;
          case Value::STRING: {
            buf = new uint8_t[2];
            buf[0] = 254;
            buf[1] = 0;
            parts.append(2, buf);
            break;
          }
          case Value::SIGNED_INT: {
            buf = new uint8_t[2];
            buf[0] = 8;
            buf[1] = 0;
            parts.append(2, buf);
            break;
          }
          case Value::UNSIGNED_INT: {
            buf = new uint8_t[2];
            buf[0] = 8;
            buf[1] = 128;
            parts.append(2, buf);
            break;
          }
          case Value::DOUBLE: {
            buf = new uint8_t[2];
            buf[0] = 5;
            buf[1] = 0;
            parts.append(2, buf);
            break;
          }
        }
      }

      // The params themselves.
      for (const Value &v : bind_params) {
        switch (v.get_type()) {
          case Value::INV_NULL:
            continue;
          case Value::STRING: {
            auto str_opt = v.get_str();
            if (str_opt.value()->size() < 0xFB) {
              buf = new uint8_t[1];
              buf[0] = static_cast<uint8_t>(str_opt.value()->size());
              parts.append(1, buf);
            } else if (str_opt.value()->size() >= 0xFB &&
                       str_opt.value()->size() <= 0xFFFF) {
              buf = new uint8_t[3];
              buf[0] = 0xFC;
              const uint16_t size =
                  static_cast<uint16_t>(str_opt.value()->size());
              const uint8_t *size_ptr =
                  reinterpret_cast<const uint8_t *>(&size);
              buf[1] = size_ptr[0];
              buf[2] = size_ptr[1];
              parts.append(3, buf);
            } else if (str_opt.value()->size() > 0xFFFF &&
                       str_opt.value()->size() <= 0xFFFFFF) {
              buf = new uint8_t[4];
              buf[0] = 0xFD;
              const uint32_t size =
                  static_cast<uint32_t>(str_opt.value()->size());
              const uint8_t *size_ptr =
                  reinterpret_cast<const uint8_t *>(&size);
              buf[1] = size_ptr[0];
              buf[2] = size_ptr[1];
              buf[3] = size_ptr[2];
              parts.append(4, buf);
            } else if (str_opt.value()->size() > 0xFFFFFF) {
              buf = new uint8_t[9];
              buf[0] = 0xFE;
              const uint64_t size = str_opt.value()->size();
              const uint8_t *size_ptr =
                  reinterpret_cast<const uint8_t *>(&size);
              buf[1] = size_ptr[0];
              buf[2] = size_ptr[1];
              buf[3] = size_ptr[2];
              buf[4] = size_ptr[3];
              buf[5] = size_ptr[4];
              buf[6] = size_ptr[5];
              buf[7] = size_ptr[6];
              buf[8] = size_ptr[7];
              parts.append(9, buf);
            } else {
              PMA_EPrintln("ERROR: Failed to bind string parameter!");
              close_stmt(stmt_id);
              return std::nullopt;
            }

            buf = new uint8_t[str_opt.value()->size()];
            std::memcpy(buf, str_opt.value()->data(), str_opt.value()->size());
            parts.append(str_opt.value()->size(), buf);
            break;
          }
          case Value::SIGNED_INT: {
            auto i_opt = v.get_signed_int();
            const uint8_t *ptr =
                reinterpret_cast<const uint8_t *>(i_opt.value().get());
            buf = new uint8_t[8];
            buf[0] = ptr[0];
            buf[1] = ptr[1];
            buf[2] = ptr[2];
            buf[3] = ptr[3];
            buf[4] = ptr[4];
            buf[5] = ptr[5];
            buf[6] = ptr[6];
            buf[7] = ptr[7];
            parts.append(8, buf);
            break;
          }
          case Value::UNSIGNED_INT: {
            auto u_opt = v.get_unsigned_int();
            const uint8_t *ptr =
                reinterpret_cast<const uint8_t *>(u_opt.value().get());
            buf = new uint8_t[8];
            buf[0] = ptr[0];
            buf[1] = ptr[1];
            buf[2] = ptr[2];
            buf[3] = ptr[3];
            buf[4] = ptr[4];
            buf[5] = ptr[5];
            buf[6] = ptr[6];
            buf[7] = ptr[7];
            parts.append(8, buf);
            break;
          }
          case Value::DOUBLE: {
            auto d_opt = v.get_double();
            const uint8_t *ptr =
                reinterpret_cast<const uint8_t *>(d_opt.value().get());
            buf = new uint8_t[8];
            buf[0] = ptr[0];
            buf[1] = ptr[1];
            buf[2] = ptr[2];
            buf[3] = ptr[3];
            buf[4] = ptr[4];
            buf[5] = ptr[5];
            buf[6] = ptr[6];
            buf[7] = ptr[7];
            parts.append(8, buf);
            break;
          }
        }
      }
    }

    PMA_HELPER::BinaryPart part = parts.combine();

    pkts = PMA_MSQL::create_packets(part.data, part.size, &seq);
  }

  // Send execute pkt(s).
  for (const Packet &pkt : pkts) {
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
#ifndef NDEBUG
          std::fprintf(stderr, "w");
#endif
          continue;
        } else {
          std::fprintf(stderr, "Failed to send execute stmt pkt (errno %d)!\n",
                       errno);
          return std::nullopt;
        }
      } else if (static_cast<size_t>(write_ret) < remaining) {
        remaining -= static_cast<size_t>(write_ret);
      } else {
#ifndef NDEBUG
        std::fprintf(stderr, "\n");
#endif
        break;
      }
    }
  }

  // Response to execute stmt pkt(s).
  bool reached_ok_eof_pkt = false;
  enum class NextPkt {
    COLUMN_COUNT,
    COLUMN_DEF,
    ROW
  } next_pkt_enum = NextPkt::COLUMN_COUNT;
  uint64_t col_count;
  std::vector<uint8_t> field_types;
  std::vector<uint16_t> field_details;
  size_t row_idx = 0;
  PMA_HELPER::BinaryPart continue_part;
  bool attempt_fetch_more = false;
  std::vector<std::vector<Value> > ret_vecs;
  while (!reached_ok_eof_pkt) {
    uint8_t buf[4096];
    size_t size = 0;
    while (true) {
      ssize_t read_ret = read(fd, buf, 4096);
      if (read_ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          if (attempt_fetch_more) {
            PMA_EPrintln("ERROR: No more bytes, but did not reach EOF!");
            return std::nullopt;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifndef NDEBUG
          std::fprintf(stderr, "r");
#endif
          continue;
        } else {
          std::fprintf(stderr,
                       "ERROR: Failed to recv after exec pkt (errno %d)!\n",
                       errno);
          return std::nullopt;
        }
      } else {
        size = static_cast<size_t>(read_ret);
#ifndef NDEBUG
        std::fprintf(stderr, "\n");
#endif
        break;
      }
    }

    if (size == 0) {
      std::fprintf(stderr,
                   "ERROR: Recv 0 bytes after sending exec stmt pkt!\n");
      return std::nullopt;
    }

#ifndef NDEBUG
    PMA_EPrintln("NOTICE: response to execute pkt size is: {}", size);
#endif

    PMA_HELPER::BinaryPart recv_part;
    {
      PMA_HELPER::BinaryParts recv_parts;
      if (continue_part.size != 0) {
        uint8_t *continue_data = new uint8_t[continue_part.size];
        std::memcpy(continue_data, continue_part.data, continue_part.size);
        recv_parts.append(continue_part.size, continue_data);
        continue_part = PMA_HELPER::BinaryPart();
      }
      uint8_t *buf_data = new uint8_t[size];
      std::memcpy(buf_data, buf, size);
      recv_parts.append(size, buf_data);
      recv_part = recv_parts.combine();
    }

    attempt_fetch_more = true;
    size_t idx = 0;

  EXECUTE_STMT_PARSE_EXECUTE_RESP_PKT:
    uint32_t pkt_size;
    uint8_t *pkt_size_bytes = reinterpret_cast<uint8_t *>(&pkt_size);
    if (recv_part.size - idx < 4) {
      size_t remaining = recv_part.size - idx;
      if (remaining != 0) {
        uint8_t *remaining_data = new uint8_t[remaining];
        std::memcpy(remaining_data, recv_part.data + idx, remaining);
        continue_part = PMA_HELPER::BinaryPart(remaining, remaining_data);
      }
#ifndef NDEBUG
      PMA_EPrintln("NOTICE: Fetching more bytes (not enough for pkt size)...");
#endif
      continue;
    }
    pkt_size_bytes[0] = recv_part.data[idx++];
    pkt_size_bytes[1] = recv_part.data[idx++];
    pkt_size_bytes[2] = recv_part.data[idx++];
    pkt_size_bytes[3] = 0;

    if (idx >= recv_part.size) {
      std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
      return std::nullopt;
    }

    uint8_t seq_id = recv_part.data[idx++];
    if (seq_id != seq) {
      std::fprintf(stderr,
                   "WARNING: execute_stmt: Recv seq %#hhx, should be %#hhx!\n",
                   seq_id, seq);
    }
    ++seq;

    if (recv_part.size - idx < static_cast<size_t>(pkt_size)) {
      idx -= 4;
      --seq;

      size_t remaining = recv_part.size - idx;
      if (remaining != 0) {
#ifndef NDEBUG
        PMA_EPrintln("Remaining bytes {}, fetching more...", remaining);
#endif
        uint8_t *remaining_data = new uint8_t[remaining];
        std::memcpy(remaining_data, recv_part.data + idx, remaining);
        continue_part = PMA_HELPER::BinaryPart(remaining, remaining_data);
      }
#ifndef NDEBUG
      PMA_EPrintln("NOTICE: Fetching more bytes (not enough for pkt)...");
#endif
      continue;
    }

    if (idx >= recv_part.size) {
      std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
      return std::nullopt;
    }

    if (recv_part.data[idx] == 0xFF) {
      std::fprintf(stderr, "ERROR: Recv Err pkt after exec pkt sent!\n");
      print_error_pkt(recv_part.data + idx, pkt_size);
      return std::nullopt;
    } else if (recv_part.data[idx] == 0xFE ||
               (recv_part.data[idx] == 0 &&
                next_pkt_enum == NextPkt::COLUMN_COUNT)) {
      const auto [ret, bytes_read] =
          handle_ok_pkt(recv_part.data + idx, pkt_size);
      idx += bytes_read;

      reached_ok_eof_pkt = true;
      if (ret != 0) {
        return std::nullopt;
      }
      continue;
    }

    switch (next_pkt_enum) {
      case NextPkt::COLUMN_COUNT: {
        auto col_count_opt =
            parse_column_count_pkt(recv_part.data + idx, pkt_size);
        if (!col_count_opt.has_value()) {
          PMA_EPrintln("ERROR: Failed to parse column-count pkt!");
          close_stmt(stmt_id);
          return std::nullopt;
        }
        col_count = col_count_opt.value();
        idx += pkt_size;
#ifndef NDEBUG
        PMA_EPrintln("NOTICE: stmt result col count: {}", col_count);
#endif
        next_pkt_enum = NextPkt::COLUMN_DEF;
        if (idx < recv_part.size) {
          goto EXECUTE_STMT_PARSE_EXECUTE_RESP_PKT;
        } else {
          continue;
        }
      }
      case NextPkt::COLUMN_DEF: {
        int ret = parse_col_type_pkt(recv_part.data + idx, pkt_size,
                                     field_types, field_details);
        if (ret != 0) {
          PMA_EPrintln("ERROR: Failed to parse column def {}!",
                       field_types.size());
          close_stmt(stmt_id);
          return std::nullopt;
        }
        idx += pkt_size;
        if (field_types.size() > col_count) {
          PMA_EPrintln(
              "ERROR: Invalid count of field types! Have {}, must be {}",
              field_types.size(), col_count);
          close_stmt(stmt_id);
          return std::nullopt;
        } else if (field_types.size() == col_count) {
          next_pkt_enum = NextPkt::ROW;
        }
        if (idx < recv_part.size) {
          goto EXECUTE_STMT_PARSE_EXECUTE_RESP_PKT;
        } else {
          continue;
        }
      }
      case NextPkt::ROW: {
#ifndef NDEBUG
        PMA_EPrintln("NOTICE: Parsing ROW {}", row_idx);
#endif
        ret_vecs.emplace_back();
        int ret = parse_row_pkt(recv_part.data + idx, pkt_size, field_types,
                                field_details, &ret_vecs.back());
        if (ret != 0) {
          PMA_EPrintln("ERROR: Failed to parse row pkt!");
          close_stmt(stmt_id);
          return std::nullopt;
        }
        ++row_idx;
        idx += pkt_size;
        if (idx < recv_part.size) {
          goto EXECUTE_STMT_PARSE_EXECUTE_RESP_PKT;
        } else {
          continue;
        }
      }
      default:
        PMA_EPrintln("ERROR: Invalid next_pkt_enum value (internal error!)");
        close_stmt(stmt_id);
        return std::nullopt;
    }
  }
  close_stmt(stmt_id);
  return ret_vecs;
}

void PMA_MSQL::Connection::close_stmt(uint32_t stmt_id) {
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
        PMA_EPrintln("WARNING: Sent {} of 9 bytes!", write_ret);
      }
      break;
    }
  }
}

std::vector<PMA_MSQL::Packet> PMA_MSQL::create_packets(uint8_t *data,
                                                       size_t data_size,
                                                       uint8_t *seq) {
  std::vector<Packet> ret;

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

std::optional<PMA_MSQL::Connection> PMA_MSQL::Connection::connect_msql(
    std::string addr, uint16_t port, std::string user, std::string pass,
    std::string dbname) {
  bool locked = Connection::m.try_lock_for(CONN_TRY_LOCK_DURATION);
  if (!locked) {
    return std::nullopt;
  }
  GenericCleanup<bool *> unlock_on_fail(&locked, [](bool **locked) {
    if (*locked && **locked) {
      Connection::m.unlock();
      **locked = false;
    }
  });

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
        std::fprintf(stderr, "r");
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
      std::fprintf(stderr, "\n");
#endif
      break;
    }
  }
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: read_ret: {}", read_ret);
#endif

  uint32_t pkt_size = static_cast<uint32_t>(buf[0]) |
                      (static_cast<uint32_t>(buf[1]) << 8) |
                      (static_cast<uint32_t>(buf[2]) << 16);
#ifndef NDEBUG
  PMA_EPrintln("pkt_size: {} ({:#x})", pkt_size, pkt_size);
#endif
  uint8_t sequence_id = buf[3];
#ifndef NDEBUG
  std::fprintf(stderr, "seq: %hhu\n", sequence_id);
#endif

  uint8_t *pkt_data = buf + 4;

  auto parse_handshake_opt = parse_init_handshake_pkt(pkt_data, pkt_size);
  if (!parse_handshake_opt.has_value()) {
    close(fd);
    PMA_EPrintln("ERROR: Failed to parse init handshake pkt!");
    return std::nullopt;
  }
  uint16_t server_caps_1;
  uint16_t server_caps_2;
  uint32_t server_caps_3;
  std::vector<uint8_t> seed;
  std::string auth_plugin_name;
  uint32_t connection_id;
  std::tie(server_caps_1, server_caps_2, server_caps_3, seed, auth_plugin_name,
           connection_id) = std::move(parse_handshake_opt.value());

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
  std::array<uint8_t, 20> auth_arr = msql_native_auth_resp(seed, pass);
  if (server_caps_2 & (1 << (21 - 16))) {
#ifndef NDEBUG
    std::fprintf(stderr, "NOTICE: LENENC auth response.\n");
#endif
    cli_buf = new uint8_t[1];
    cli_buf[0] = 20;
    parts.append(1, cli_buf);

    cli_buf = new uint8_t[auth_arr.size()];
    std::memcpy(cli_buf, auth_arr.data(), auth_arr.size());
    parts.append(auth_arr.size(), cli_buf);
  } else if (server_caps_1 & (1 << 15)) {
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

  if (server_caps_1 & 8) {
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
  PMA_EPrintln("NOTICE: Client send size: {}", combined.size);
#endif
  sequence_id += 1;
  std::vector<Packet> write_pkts =
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
          std::fprintf(stderr, "w");
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
#ifndef NDEBUG
    std::fprintf(stderr, "\n");
#endif
  }

  while (true) {
    read_ret = read(fd, buf, 4096);
    if (read_ret == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // NONBLOCKING nothing to read.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
#ifndef NDEBUG
        std::fprintf(stderr, "r");
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
      std::fprintf(stderr, "\n");
#endif
      break;
    }
  }
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: read_ret: {}", read_ret);
#endif

  pkt_size = static_cast<uint32_t>(buf[0]) |
             (static_cast<uint32_t>(buf[1]) << 8) |
             (static_cast<uint32_t>(buf[2]) << 16);
#ifndef NDEBUG
  PMA_EPrintln("pkt_size: {} ({:#x})", pkt_size, pkt_size);
#endif
  sequence_id = buf[3];
#ifndef NDEBUG
  PMA_EPrintln("seq: {}", sequence_id);
#endif

  pkt_data = buf + 4;
  size_t idx = 0;

  // "Header" byte.
  if (server_caps_2 & (1 << (24 - 16))) {
    // CLIENT_DEPRECATE_EOF set.
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

  const auto [handle_status, ok_bytes_read] = handle_ok_pkt(pkt_data, pkt_size);
  if (handle_status != 0) {
    close(fd);
    PMA_EPrintln("ERROR: Failed to handle ok packet after init handshake!");
    return std::nullopt;
  }

  // Prevent unlock, Connection implicitly owns the lock now.
  locked = false;
  return Connection(fd, connection_id);
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

std::tuple<int, size_t> PMA_MSQL::handle_ok_pkt(uint8_t *buf, size_t size) {
  size_t idx = 0;
  if (buf[idx] != 0 && buf[idx] != 0xFE) {
    PMA_EPrintln("ERROR: First byte of ok packet is not 0/0xFE!");
    return {1, 1};
  }
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: Handling OK/EOF packet.");
#endif
  ++idx;
  if (idx >= size) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return {1, idx};
  }

  uint64_t rows_size;
  uint_fast8_t bytes_read;
  std::tie(rows_size, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  if (idx >= size) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return {1, idx};
  }
#ifndef NDEBUG
  PMA_EPrintln("stmt affected {} rows!", rows_size);
#endif

  uint64_t last_insert_id;
  std::tie(last_insert_id, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  if (idx >= size) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return {1, idx};
  }
#ifndef NDEBUG
  PMA_EPrintln("last insert id: {}", last_insert_id);
#endif

  uint16_t status;
  uint8_t *status_bytes = reinterpret_cast<uint8_t *>(&status);
  status_bytes[0] = buf[idx++];
  status_bytes[1] = buf[idx++];
  if (idx >= size) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return {1, idx};
  }
#ifndef NDEBUG
  PMA_EPrintln("Server status: {:#x}", status);
#endif

  uint16_t warning_c;
  uint8_t *warning_c_bytes = reinterpret_cast<uint8_t *>(&warning_c);
  warning_c_bytes[0] = buf[idx++];
  warning_c_bytes[1] = buf[idx++];
  if (idx == size) {
    return {0, idx};
  }

  if (idx >= size) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return {1, idx};
  }

#ifndef NDEBUG
  PMA_EPrintln("Warning count: {}", warning_c);
#endif

  uint64_t info_string_size;
  std::tie(info_string_size, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;

  if (idx + info_string_size > size) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return {1, idx};
  }

  std::string str(reinterpret_cast<char *>(buf + idx), info_string_size);

  // TODO Determine if this is necessary.
  // PMA_EPrintln("Info string: {}", str);
  return {0, idx + info_string_size};
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

    uint64_t str_len;
    uint_fast8_t bytes_read;
    std::tie(str_len, bytes_read) = parse_len_enc_int(data + idx);
    idx += bytes_read;
    std::string progress(reinterpret_cast<char *>(data + idx), str_len);
    PMA_EPrintln("Progress String: {}", progress);
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

std::optional<std::tuple<uint16_t, uint16_t, uint32_t, std::vector<uint8_t>,
                         std::string, uint32_t> >
PMA_MSQL::parse_init_handshake_pkt(uint8_t *data, size_t size) {
  size_t idx = 0;
  // Protocol version.
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: Protocol version: {}", data[idx]);
#endif
  ++idx;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server version.
  std::fprintf(stderr, "NOTICE: Connecting to server, reported version: %s\n",
               data + idx);
  while (data[idx] != 0 && idx < size) {
    ++idx;
  }
  ++idx;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: idx after server version: {}", idx);
#endif

  // Connection id.
  uint32_t connection_id = static_cast<uint32_t>(data[idx]) |
                           (static_cast<uint32_t>(data[idx + 1]) << 8) |
                           (static_cast<uint32_t>(data[idx + 2]) << 16) |
                           (static_cast<uint32_t>(data[idx + 3]) << 24);
  idx += 4;
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: Connection id {} ({:#x})", connection_id,
               connection_id);
#endif
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Auth plugin data.
  std::unique_ptr<uint8_t[]> auth_plugin_data = std::make_unique<uint8_t[]>(64);
  std::memcpy(auth_plugin_data.get(), data + idx, 8);
  idx += 8;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Reserved byte.
  idx += 1;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server capabilities (1st part).
  uint16_t server_capabilities_1 = *reinterpret_cast<uint16_t *>(data + idx);
  server_capabilities_1 = PMA_HELPER::le_swap_u16(server_capabilities_1);
#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: Server capabilities 1: %#hx\n",
               server_capabilities_1);
#endif
  idx += 2;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server default collation.
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: Server default collation: {} ({:#x})", data[idx],
               data[idx]);
#endif
  idx += 1;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // status flags.
  idx += 2;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Server capabilities (2nd part).
  uint16_t server_capabilities_2 = *reinterpret_cast<uint16_t *>(data + idx);
  server_capabilities_2 = PMA_HELPER::le_swap_u16(server_capabilities_2);
#ifndef NDEBUG
  PMA_EPrintln("NOTICE: Server capabilities 2: {:#x}", server_capabilities_2);
#endif
  idx += 2;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Plugin auth.
  uint8_t plugin_data_length = 0;
  if (server_capabilities_2 & 0x8) {
    plugin_data_length = data[idx];
    idx += 1;
#ifndef NDEBUG
    PMA_EPrintln("NOTICE: plugin_data_length: {}", plugin_data_length);
#endif
  } else {
    idx += 1;
  }
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // Filler
  idx += 6;
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  // CLIENT_MYSQL or server_capabilities_3
  uint32_t server_capabilities_3 = 0;
  if (server_capabilities_1 & 1) {
    // filler
    idx += 4;
  } else {
    server_capabilities_3 = *reinterpret_cast<uint32_t *>(data + idx);
    server_capabilities_3 = PMA_HELPER::le_swap_u32(server_capabilities_3);
    idx += 4;
  }
  if (idx >= size) {
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
    PMA_EPrintln("NOTICE: Writing size {} to auth_plugin_data offset 8",
                 size_max);
#endif
    std::memcpy(auth_plugin_data.get() + 8, data + idx, size_max);
    idx += size_max;
    idx += 1;
  }
  if (idx >= size) {
    std::fprintf(stderr, "idx (%zd) out of bounds: %u\n", idx, __LINE__);
    return std::nullopt;
  }

  std::string auth_plugin_name;
  if (server_capabilities_2 & 0x8) {
#ifndef NDEBUG
    PMA_EPrintln("NOTICE: at auth_plugin_name: pkt_size {}, idx {}", size, idx);
#endif
    auto str_size =
        static_cast<std::string::allocator_type::size_type>(size - idx);
    auth_plugin_name =
        std::string(reinterpret_cast<char *>(data + idx), str_size);
#ifndef NDEBUG
    PMA_EPrintln("NOTICE: auth_plugin_name: {}", auth_plugin_name);
#endif
  }

  std::vector<uint8_t> seed;
  for (size_t sidx = 0; sidx < auth_plugin_data_size; ++sidx) {
    seed.push_back(auth_plugin_data[sidx]);
  }

  return std::make_tuple(server_capabilities_1, server_capabilities_2,
                         server_capabilities_3, seed, auth_plugin_name,
                         connection_id);
}

std::optional<std::tuple<int, uint32_t> > PMA_MSQL::parse_prepare_resp_pkt(
    uint8_t *buf, size_t size) {
  size_t idx = 0;
  if (buf[idx] != 0) {
    return std::nullopt;
  }
  ++idx;

  uint32_t stmt_id;
  uint8_t *stmt_id_bytes = reinterpret_cast<uint8_t *>(&stmt_id);
  stmt_id_bytes[0] = buf[idx++];
  stmt_id_bytes[1] = buf[idx++];
  stmt_id_bytes[2] = buf[idx++];
  stmt_id_bytes[3] = buf[idx++];

#ifndef NDEBUG
  std::fprintf(stderr, "NOTICE: stmt id %" PRIu32 " (%#" PRIx32 ")\n", stmt_id,
               stmt_id);
#endif

  if (idx >= size) {
    std::fprintf(stderr, "ERROR: execute_stmt: Recv idx out of bounds!\n");
    return std::make_tuple(1, stmt_id);
  }

  uint16_t cols;
  uint8_t *cols_bytes = reinterpret_cast<uint8_t *>(&cols);
  cols_bytes[0] = buf[idx++];
  cols_bytes[1] = buf[idx++];
  if (cols != 0) {
    std::fprintf(stderr, "WARNING: Got non-zero cols %" PRIu16 "!\n", cols);
  }

  if (idx >= size) {
    std::fprintf(stderr, "ERROR: execute_stmt: Recv idx out of bounds!\n");
    return std::make_tuple(1, stmt_id);
  }

  uint16_t params;
  uint8_t *params_bytes = reinterpret_cast<uint8_t *>(&params);
  params_bytes[0] = buf[idx++];
  params_bytes[1] = buf[idx++];

  // unused 1 byte.
  ++idx;

  if (idx >= size) {
    std::fprintf(stderr, "ERROR: execute_stmt: Recv idx out of bounds!\n");
    return std::make_tuple(1, stmt_id);
  }

  uint16_t warnings;
  uint8_t *warn_bytes = reinterpret_cast<uint8_t *>(&warnings);
  warn_bytes[0] = buf[idx++];
  warn_bytes[1] = buf[idx++];

  if (warnings > 0) {
    std::fprintf(stderr, "NOTICE: %" PRIu16 " warnings!\n", warnings);
  }

  return std::make_tuple(0, stmt_id);
}

std::optional<uint64_t> PMA_MSQL::parse_column_count_pkt(uint8_t *buf,
                                                         size_t size) {
  uint64_t count;
  uint_fast8_t bytes_read;
  std::tie(count, bytes_read) = parse_len_enc_int(buf);

  if (bytes_read != size) {
    return std::nullopt;
  }

  return count;
}

int PMA_MSQL::parse_col_type_pkt(uint8_t *buf, size_t size,
                                 std::vector<uint8_t> &field_types,
                                 std::vector<uint16_t> &field_details) {
  size_t idx = 0;
  uint64_t catalog_len;
  uint_fast8_t bytes_read;
  std::tie(catalog_len, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  if (catalog_len != 3) {
    PMA_EPrintln("ERROR: Catalog len is not 3! (is {}, {:#x})", catalog_len,
                 catalog_len);
    return 1;
  }
  std::string catalog(reinterpret_cast<char *>(buf + idx), 3);
  if (catalog != "def") {
    PMA_EPrintln("ERROR: Catalog is not \"def\"! (is \"{}\")", catalog);
    return 1;
  }
  idx += 3;

  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  // schema
  uint64_t schema_len;
  std::tie(schema_len, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  std::string temp_str(reinterpret_cast<char *>(buf + idx), schema_len);
  idx += schema_len;
#ifndef NDEBUG
  PMA_EPrintln("Schema size: {}; Schema: {}", temp_str.size(), temp_str);
#endif
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  // table alias
  uint64_t table_alias_len;
  std::tie(table_alias_len, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  temp_str = std::string(reinterpret_cast<char *>(buf + idx), table_alias_len);
  idx += table_alias_len;
#ifndef NDEBUG
  PMA_EPrintln("Table alias size: {}; Table alias: {}", temp_str.size(),
               temp_str);
#endif
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  // table
  uint64_t table_len;
  std::tie(table_len, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  temp_str = std::string(reinterpret_cast<char *>(buf + idx), table_len);
  idx += table_len;
#ifndef NDEBUG
  PMA_EPrintln("Table name size: {}; Table name: {}", temp_str.size(),
               temp_str);
#endif
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  // column alias
  uint64_t column_alias_len;
  std::tie(column_alias_len, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  temp_str = std::string(reinterpret_cast<char *>(buf + idx), column_alias_len);
  idx += column_alias_len;
#ifndef NDEBUG
  PMA_EPrintln("Column alias size: {}; Column alias: {}", temp_str.size(),
               temp_str);
#endif
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  // column
  uint64_t column_name_len;
  std::tie(column_name_len, bytes_read) = parse_len_enc_int(buf + idx);
  idx += bytes_read;
  temp_str = std::string(reinterpret_cast<char *>(buf + idx), column_name_len);
  idx += column_name_len;
#ifndef NDEBUG
  PMA_EPrintln("Column name size: {}; Column name: {}", temp_str.size(),
               temp_str);
#endif
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  // Constant 0xc
  if (buf[idx] != 0xc) {
    PMA_EPrintln("ERROR: Expected 0xC, got {:#x}", buf[idx]);
    return 1;
  }
  ++idx;
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  uint16_t char_set;
  uint8_t *char_set_bytes = reinterpret_cast<uint8_t *>(&char_set);
  char_set_bytes[0] = buf[idx++];
  char_set_bytes[1] = buf[idx++];
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }
#ifndef NDEBUG
  PMA_EPrintln("Server char-set: {} ({:#x})", char_set, char_set);
#endif

  uint32_t max_col_size;
  uint8_t *max_col_size_bytes = reinterpret_cast<uint8_t *>(&max_col_size);
  max_col_size_bytes[0] = buf[idx++];
  max_col_size_bytes[1] = buf[idx++];
  max_col_size_bytes[2] = buf[idx++];
  max_col_size_bytes[3] = buf[idx++];
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }
#ifndef NDEBUG
  PMA_EPrintln("Max column size: {}", max_col_size);
#endif

  uint8_t field_type = buf[idx++];
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }
#ifndef NDEBUG
  PMA_EPrintln("Field type is {} ({:#x})", field_type, field_type);
#endif
  field_types.push_back(field_type);

  uint16_t field_detail;
  uint8_t *field_detail_bytes = reinterpret_cast<uint8_t *>(&field_detail);
  field_detail_bytes[0] = buf[idx++];
  field_detail_bytes[1] = buf[idx++];
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }
#ifndef NDEBUG
  PMA_EPrintln("Field detail flag: {:#x}", field_detail);
#endif
  field_details.push_back(field_detail);

  uint8_t decimals = buf[idx++];
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }
#ifndef NDEBUG
  PMA_EPrintln("Decimals: {}", decimals);
#endif

  // Unused 2 bytes.
  idx += 2;
  if (idx > size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  if (idx != size) {
    PMA_EPrintln("WARNING: idx does not match end of field pkt!");
  }
  return 0;
}

int PMA_MSQL::parse_row_pkt(uint8_t *buf, size_t size,
                            const std::vector<uint8_t> &field_types,
                            const std::vector<uint16_t> &field_details,
                            std::vector<Value> *out) {
  size_t idx = 0;

  if (buf[idx] != 0) {
    return 1;
  }

  ++idx;
  size_t null_bitmap_size = (7 + 2 + field_types.size()) / 8;

  uint8_t *bitmap_buf = nullptr;
  GenericCleanup<uint8_t **> bitmap_buf_cleanup(&bitmap_buf,
                                                [](uint8_t ***ptr) {
                                                  if (ptr && *ptr && **ptr) {
                                                    delete[] **ptr;
                                                    **ptr = nullptr;
                                                  }
                                                });
  if (null_bitmap_size != 0) {
    bitmap_buf = new uint8_t[null_bitmap_size];
    for (size_t bidx = 0; bidx < null_bitmap_size; ++bidx) {
      bitmap_buf[bidx] = buf[idx++];
    }
  }
  if (idx >= size || idx >= 4096) {
    std::fprintf(stderr, "ERROR: Recv after exec parsing out of bounds!\n");
    return 1;
  }

  for (size_t bidx = 0; bidx < field_types.size(); ++bidx) {
    size_t buf_idx = 0;
    size_t bidx_adjusted = bidx;
    while (bidx_adjusted + 2 >= 8) {
      ++buf_idx;
      bidx_adjusted -= 8;
    }
    if (bitmap_buf[buf_idx] & (1 << (bidx_adjusted + 2))) {
      // Field is NULL, do nothing.
#ifndef NDEBUG
      PMA_EPrintln(" Col {} is NULL", bidx);
#endif
      if (out) {
        out->emplace_back();
      }
    } else {
#ifndef NDEBUG
      PMA_EPrintln(" Col {} is NOT NULL", bidx);
#endif
      // Field is NOT NULL.
      uint_fast8_t bytes_read;
      switch (field_types.at(bidx)) {
        case 0: {
          uint64_t decimal_length;
          std::tie(decimal_length, bytes_read) = parse_len_enc_int(buf + idx);
          idx += bytes_read;
          PMA_EPrintln("WARNING: Unhandled \"DECIMAL\" of size {}",
                       decimal_length);
          idx += decimal_length;
          if (idx > size || idx >= 4096) {
            std::fprintf(stderr,
                         "ERROR: Recv after exec parsing out of bounds!\n");
            return 1;
          }
          PMA_EPrintln("WARNING: \"DECIMAL\" handling implementation pending!");
          if (out) {
            out->emplace_back();
          }
          break;
        }
        case 1: {
          if (field_details.at(bidx) & 0x20) {
            uint8_t tiny = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value TINY: {:d}", tiny);
#endif
            if (out) {
              out->emplace_back(static_cast<uint64_t>(tiny));
            }
          } else {
            int8_t tiny = static_cast<int8_t>(buf[idx++]);
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value TINY: {:d}", tiny);
#endif
            if (out) {
              out->emplace_back(static_cast<int64_t>(tiny));
            }
          }
          break;
        }
        case 2: {
          if (field_details.at(bidx) & 0x20) {
            uint16_t small;
            uint8_t *small_bytes = reinterpret_cast<uint8_t *>(&small);
            small_bytes[0] = buf[idx++];
            small_bytes[1] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value SMALL: {}", small);
#endif
            if (out) {
              out->emplace_back(static_cast<uint64_t>(small));
            }
          } else {
            int16_t small;
            uint8_t *small_bytes = reinterpret_cast<uint8_t *>(&small);
            small_bytes[0] = buf[idx++];
            small_bytes[1] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value SMALL: {}", small);
#endif
            if (out) {
              out->emplace_back(static_cast<int64_t>(small));
            }
          }
          break;
        }
        case 3: {
          if (field_details.at(bidx) & 0x20) {
            uint32_t long_int;
            uint8_t *long_bytes = reinterpret_cast<uint8_t *>(&long_int);
            long_bytes[0] = buf[idx++];
            long_bytes[1] = buf[idx++];
            long_bytes[2] = buf[idx++];
            long_bytes[3] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value LONG: {}", long_int);
#endif
            if (out) {
              out->emplace_back(static_cast<uint64_t>(long_int));
            }
          } else {
            int32_t long_int;
            uint8_t *long_bytes = reinterpret_cast<uint8_t *>(&long_int);
            long_bytes[0] = buf[idx++];
            long_bytes[1] = buf[idx++];
            long_bytes[2] = buf[idx++];
            long_bytes[3] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value LONG: {}", long_int);
#endif
            if (out) {
              out->emplace_back(static_cast<int64_t>(long_int));
            }
          }
          break;
        }
        case 4: {
          float float_val;
          uint8_t *float_bytes = reinterpret_cast<uint8_t *>(&float_val);
          float_bytes[0] = buf[idx++];
          float_bytes[1] = buf[idx++];
          float_bytes[2] = buf[idx++];
          float_bytes[3] = buf[idx++];
          if (idx > size || idx >= 4096) {
            std::fprintf(stderr,
                         "ERROR: Recv after exec parsing out of bounds!\n");
            return 1;
          }
#ifndef NDEBUG
          PMA_EPrintln("  Value FLOAT: {}", float_val);
#endif
          if (out) {
            out->emplace_back(float_val);
          }
          break;
        }
        case 5: {
          double double_val;
          uint8_t *double_bytes = reinterpret_cast<uint8_t *>(&double_val);
          double_bytes[0] = buf[idx++];
          double_bytes[1] = buf[idx++];
          double_bytes[2] = buf[idx++];
          double_bytes[3] = buf[idx++];
          double_bytes[4] = buf[idx++];
          double_bytes[5] = buf[idx++];
          double_bytes[6] = buf[idx++];
          double_bytes[7] = buf[idx++];
          if (idx > size || idx >= 4096) {
            std::fprintf(stderr,
                         "ERROR: Recv after exec parsing out of bounds!\n");
            return 1;
          }
#ifndef NDEBUG
          PMA_EPrintln("  Value DOUBLE: {}", double_val);
#endif
          if (out) {
            out->emplace_back(double_val);
          }
          break;
        }
        case 6:
          PMA_EPrintln("ERROR: Invalid type NULL!");
          return 1;
        case 7:
          PMA_EPrintln("ERROR: Unimplemented handling of TIMESTAMP!");
          return 1;
        case 8: {
          if (field_details.at(bidx) & 0x20) {
            uint64_t long_long;
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&long_long);
            bytes[0] = buf[idx++];
            bytes[1] = buf[idx++];
            bytes[2] = buf[idx++];
            bytes[3] = buf[idx++];
            bytes[4] = buf[idx++];
            bytes[5] = buf[idx++];
            bytes[6] = buf[idx++];
            bytes[7] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value LONGLONG: {}", long_long);
#endif
            if (out) {
              out->emplace_back(long_long);
            }
          } else {
            int64_t long_long;
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&long_long);
            bytes[0] = buf[idx++];
            bytes[1] = buf[idx++];
            bytes[2] = buf[idx++];
            bytes[3] = buf[idx++];
            bytes[4] = buf[idx++];
            bytes[5] = buf[idx++];
            bytes[6] = buf[idx++];
            bytes[7] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value LONGLONG: {}", long_long);
#endif
            if (out) {
              out->emplace_back(long_long);
            }
          }
          break;
        }
        case 9: {
          if (field_details.at(bidx) & 0x20) {
            uint32_t int24;
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&int24);
            bytes[0] = buf[idx++];
            bytes[1] = buf[idx++];
            bytes[2] = buf[idx++];
            bytes[3] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value INT24: {}", int24);
#endif
            if (out) {
              out->emplace_back(static_cast<uint64_t>(int24));
            }
          } else {
            int32_t int24;
            uint8_t *bytes = reinterpret_cast<uint8_t *>(&int24);
            bytes[0] = buf[idx++];
            bytes[1] = buf[idx++];
            bytes[2] = buf[idx++];
            bytes[3] = buf[idx++];
            if (idx > size || idx >= 4096) {
              std::fprintf(stderr,
                           "ERROR: Recv after exec parsing out of bounds!\n");
              return 1;
            }
#ifndef NDEBUG
            PMA_EPrintln("  Value INT24: {}", int24);
#endif
            if (out) {
              out->emplace_back(static_cast<int64_t>(int24));
            }
          }
          break;
        }
        case 252: {
          const auto [value, b_read] = parse_len_enc_int(buf + idx);
          idx += b_read;
          std::string str(reinterpret_cast<char *>(buf + idx), value);
          idx += value;
#ifndef NDEBUG
          PMA_EPrintln("  Value Blob: {}", str);
#endif
          if (out) {
            out->emplace_back(std::move(str));
          }
          break;
        }
        case 254: {
          const auto [value, b_read] = parse_len_enc_int(buf + idx);
          idx += b_read;
          std::string str(reinterpret_cast<char *>(buf + idx), value);
          idx += value;
#ifndef NDEBUG
          PMA_EPrintln("  Value String: {}", str);
#endif
          if (out) {
            out->emplace_back(std::move(str));
          }
          break;
        }
        default:
          PMA_EPrintln("ERROR: Unhandled field type");
          return 1;
      }
    }
  }
  return 0;
}

void PMA_MSQL::init_db(PMA_MSQL::Connection &c) {
  if (!c.is_valid()) {
    return;
  }

  c.execute_stmt(DB_INIT_TABLE_SEQ_ID, {});
  c.execute_stmt(DB_INIT_TABLE_CHALLENGE_FACTORS, {});
  c.execute_stmt(DB_INIT_TABLE_ALLOWED_IPS, {});
  c.execute_stmt(DB_INIT_TABLE_ID_TO_PORT, {});
}

std::optional<uint64_t> PMA_MSQL::get_next_seq_id(Connection &c) {
  if (!c.is_valid()) {
    return std::nullopt;
  }

  auto exec_ret = c.execute_stmt("START TRANSACTION", {});
  if (!exec_ret.has_value()) {
    PMA_EPrint("ERROR: Failed to START TRANSACTION; get next seq id");
    return std::nullopt;
  }

  exec_ret = c.execute_stmt(DB_GET_SEQ_ID, {});
  if (!exec_ret.has_value()) {
    c.execute_stmt("ROLLBACK", {});
    PMA_EPrintln("ERROR: Failed to fetch seq id from msql db!");
    return std::nullopt;
  } else if (exec_ret->empty()) {
    uint64_t seq = 0;
    if (!c.execute_stmt(DB_ADD_SEQ_ID, {seq + 1}).has_value()) {
      c.execute_stmt("ROLLBACK", {});
      PMA_EPrintln("ERROR: Failed to add seq id to msql db!");
      return std::nullopt;
    }
    c.execute_stmt("COMMIT", {});
    return seq;
  } else if (exec_ret->size() > 1) {
    std::vector<Value> last = exec_ret->back();
    exec_ret->pop_back();
    for (std::vector<Value> &row : exec_ret.value()) {
      c.execute_stmt(DB_REMOVE_SEQ_ID, {row.at(0)});
    }
    if (last.at(1).get_type() == Value::UNSIGNED_INT) {
      uint64_t seq = *last.at(1).get_unsigned_int().value();
      if (!c.execute_stmt(DB_UPDATE_SEQ_ID, {seq + 1}).has_value()) {
        c.execute_stmt("ROLLBACK", {});
        PMA_EPrintln("ERROR: Failed to UPDATE SEQ_ID!");
        return std::nullopt;
      }
      c.execute_stmt("COMMIT", {});
      return seq;
    } else {
      c.execute_stmt("ROLLBACK", {});
      PMA_EPrintln("ERROR: SEQ_ID in DB is not Unsigned!");
      return std::nullopt;
    }
  } else {
    if (exec_ret->at(0).at(1).get_type() == Value::UNSIGNED_INT) {
      uint64_t seq = *exec_ret->at(0).at(1).get_unsigned_int().value();
      if (!c.execute_stmt(DB_UPDATE_SEQ_ID, {seq + 1}).has_value()) {
        c.execute_stmt("ROLLBACK", {});
        PMA_EPrintln("ERROR: Failed to UPDATE SEQ_ID!");
        return std::nullopt;
      }
      c.execute_stmt("COMMIT", {});
      return seq;
    } else {
      c.execute_stmt("ROLLBACK", {});
      PMA_EPrintln("ERROR: SEQ_ID in DB is not Unsigned!");
      return std::nullopt;
    }
  }
}

std::optional<bool> PMA_MSQL::has_challenge_factor_id(Connection &c,
                                                      std::string hash) {
  if (!c.is_valid()) {
    return std::nullopt;
  }

  auto vec_opt = c.execute_stmt(DB_SEL_CHAL_FACT_BY_ID, {hash});
  if (vec_opt.has_value()) {
    if (vec_opt.value().size() == 1) {
      return true;
    } else {
      return false;
    }
  } else {
    return std::nullopt;
  }
}

std::optional<bool> PMA_MSQL::set_challenge_factor(Connection &c,
                                                   std::string ip,
                                                   std::string hash,
                                                   uint16_t port,
                                                   std::string factors_hash) {
  if (!c.is_valid()) {
    return std::nullopt;
  }

  if (!c.execute_stmt("LOCK TABLE CXX_CHALLENGE_FACTORS WRITE", {})
           .has_value()) {
    PMA_EPrintln(
        "ERROR: Failed to lock db table challenge factors for writing!");
    return std::nullopt;
  } else if (!c.execute_stmt(DB_ADD_CHAL_FACT,
                             {hash, ip, Value::new_uint(port), factors_hash})
                  .has_value()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Failed to add to challenge factors!");
    return std::nullopt;
  }

  c.execute_stmt("UNLOCK TABLES", {});
  return true;
}

std::optional<uint16_t> PMA_MSQL::get_id_to_port_port(Connection &c,
                                                      std::string id) {
  if (!c.is_valid()) {
    return std::nullopt;
  }

  if (!c.execute_stmt("LOCK TABLE CXX_ID_TO_PORT WRITE", {}).has_value()) {
    PMA_EPrintln("ERROR: Failed to lock db table ID_TO_PORT for writing!");
    return std::nullopt;
  }

  auto vec_opt = c.execute_stmt(DB_GET_PORT_ID_TO_PORT, {id});
  if (!vec_opt.has_value()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Failed to select port from ID_TO_PORT!");
    return std::nullopt;
  } else if (vec_opt->empty()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Port from ID_TO_PORT not found with given id!");
    return std::nullopt;
  } else if (vec_opt->at(0).at(0).get_type() == Value::UNSIGNED_INT) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Port from ID_TO_PORT is not unsigned int!");
    return std::nullopt;
  }

  uint16_t port =
      static_cast<uint16_t>(*vec_opt->at(0).at(0).get_unsigned_int().value());
  vec_opt = c.execute_stmt(DB_DEL_ID_TO_PORT_ENTRY, {id});
  if (!vec_opt.has_value()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Failed to delete entry from ID_TO_PORT!");
    return std::nullopt;
  }

  c.execute_stmt("UNLOCK TABLES", {});
  return port;
}

std::optional<std::tuple<bool, uint16_t> > PMA_MSQL::validate_client(
    Connection &c, uint64_t cleanup_minutes, std::string id,
    std::string factors_hash, std::string client_ip) {
  if (!c.is_valid()) {
    return std::nullopt;
  }

  if (!c.execute_stmt("LOCK TABLE CXX_CHALLENGE_FACTORS WRITE", {})
           .has_value()) {
    PMA_EPrintln(
        "ERROR: Failed to lock table challenge factors while validating "
        "client!");
    return std::nullopt;
  }

  // cleanup first.
  if (!c.execute_stmt(DB_CLEANUP_CHAL_FACT, {cleanup_minutes})) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln(
        "ERROR: Failed to cleanup challenge factors while validating client!");
    return std::nullopt;
  }

  // validate.
  auto vec_opt = c.execute_stmt(DB_IP_PORT_FROM_CHAL_FACT, {id, factors_hash});
  if (!vec_opt.has_value()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Failed to validate client; failed to fetch from db!");
    return std::nullopt;
  } else if (vec_opt->empty()) {
    c.execute_stmt("UNLOCK TABLES", {});
    return std::make_tuple(false, 0);
  } else if (vec_opt->at(0).at(0).get_type() != Value::STRING) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln(
        "ERROR: Failed to validate client; First col is not a String!");
    return std::nullopt;
  } else if (vec_opt->at(0).at(1).get_type() != Value::UNSIGNED_INT) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln(
        "ERROR: Failed to validate client; Second col is not an Unsigned Int!");
    return std::nullopt;
  } else if (*vec_opt->at(0).at(0).get_str().value() != client_ip) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln(
        "ERROR: Matching factors hash, but ip address does not match req ip "
        "{}!",
        client_ip);
    return std::nullopt;
  }
  uint16_t port =
      static_cast<uint16_t>(*vec_opt->at(0).at(1).get_unsigned_int().value());
  c.execute_stmt(DB_DEL_FROM_CHAL_FACT, {id});
  c.execute_stmt("UNLOCK TABLES", {});

  vec_opt = c.execute_stmt(DB_ADD_ALLOWED_IPS_ENTRY,
                           {client_ip, Value::new_uint(port)});
  if (!vec_opt.has_value()) {
    PMA_EPrintln("ERROR: Failed to add entry to Allowed IPs table for ip {}!",
                 client_ip);
    return std::nullopt;
  }

  return std::make_tuple(true, port);
}

std::optional<bool> PMA_MSQL::client_is_allowed(Connection &c, std::string ip,
                                                uint16_t port,
                                                uint64_t minutes_timeout) {
  if (!c.is_valid()) {
    return std::nullopt;
  }

  if (!c.execute_stmt("LOCK TABLE CXX_ALLOWED_IPS WRITE", {}).has_value()) {
    PMA_EPrintln("ERROR: Failed to lock Allowed IPs table (write)!");
    return std::nullopt;
  } else if (!c.execute_stmt(DB_CLEANUP_ALLOWED_IPS, {minutes_timeout})
                  .has_value()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Failed to cleanup Allowed IPs table!");
    return std::nullopt;
  }

  c.execute_stmt("UNLOCK TABLES", {});
  if (!c.execute_stmt("LOCK TABLE CXX_ALLOWED_IPS READ", {}).has_value()) {
    PMA_EPrintln("ERROR: Failed to lock Allowed IPs table (read)!");
    return std::nullopt;
  }

  auto vec_opt = c.execute_stmt(DB_IS_ALLOWED_IPS, {ip, Value::new_uint(port)});
  if (!vec_opt.has_value()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Failed to get from Allowed IPs table!");
    return std::nullopt;
  } else if (vec_opt->empty()) {
    c.execute_stmt("UNLOCK TABLES", {});
    return false;
  } else {
    c.execute_stmt("UNLOCK TABLES", {});
    return true;
  }
}

std::optional<std::string> PMA_MSQL::init_id_to_port(Connection &c,
                                                     uint16_t port,
                                                     uint64_t minutes_timeout) {
  if (!c.is_valid()) {
    return std::nullopt;
  }

  if (!c.execute_stmt("LOCK TABLE CXX_ID_TO_PORT WRITE", {}).has_value()) {
    PMA_EPrintln("ERROR: Failed to lcok ID to Port table (write)!");
    return std::nullopt;
  }

  c.execute_stmt(DB_CLEANUP_ID_TO_PORT, {minutes_timeout});

  bool same_id_exists = true;
  std::string id_hashed;
  do {
    std::optional<std::uint64_t> seq_next_opt = get_next_seq_id(c);
    if (!seq_next_opt.has_value()) {
      c.execute_stmt("UNLOCK TABLES", {});
      PMA_EPrintln("ERROR: Failed to get next seq id (init id to port)!");
      return std::nullopt;
    }
    id_hashed = PMA_SQL::next_hash(seq_next_opt.value());
    auto vec_opt = c.execute_stmt(DB_GET_PORT_ID_TO_PORT, {id_hashed});
    if (!vec_opt.has_value()) {
      c.execute_stmt("UNLOCK TABLES", {});
      PMA_EPrintln("ERROR: Failed to check next seq id (init id to port)!");
      return std::nullopt;
    } else if (vec_opt->empty()) {
      same_id_exists = false;
    }
  } while (same_id_exists);

  if (!c.execute_stmt(DB_ADD_ID_TO_PORT, {id_hashed, Value::new_uint(port)})
           .has_value()) {
    c.execute_stmt("UNLOCK TABLES", {});
    PMA_EPrintln("ERROR: Failed to add new id-to-port entry!");
    return std::nullopt;
  }

  return id_hashed;
}
