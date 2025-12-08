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

// Standard library includes.
#include <array>
#include <bitset>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Local includes.
#include "helpers.h"

namespace PMA_MSQL {

struct Packet {
  Packet();
  ~Packet();

  Packet(uint32_t, uint8_t, uint8_t *);

  // No copy
  Packet(const Packet &) = delete;
  Packet &operator=(const Packet &) = delete;

  // Allow move
  Packet(Packet &&);
  Packet &operator=(Packet &&);

  // Treat this as 3 bytes.
  uint32_t packet_length : 24;
  uint8_t seq;
  uint8_t *body;
};

class Value {
 public:
  Value();
  Value(std::string str);
  Value(int64_t sint);
  Value(uint64_t uint);
  Value(double d);

  ~Value();

  // allow copy
  Value(const Value &);
  Value &operator=(const Value &);

  // allow move
  Value(Value &&);
  Value &operator=(Value &&);

  // Helpers to specify specific int types.
  static Value new_int(int64_t i);
  static Value new_uint(uint64_t u);

  enum TypeE { INV_NULL, STRING, SIGNED_INT, UNSIGNED_INT, DOUBLE };

  TypeE get_type() const;

  std::optional<std::shared_ptr<std::string> > get_str();
  std::optional<std::shared_ptr<int64_t> > get_signed_int();
  std::optional<std::shared_ptr<uint64_t> > get_unsigned_int();
  std::optional<std::shared_ptr<double> > get_double();

  std::optional<std::shared_ptr<const std::string> > get_str() const;
  std::optional<std::shared_ptr<const int64_t> > get_signed_int() const;
  std::optional<std::shared_ptr<const uint64_t> > get_unsigned_int() const;
  std::optional<std::shared_ptr<const double> > get_double() const;

 private:
  union U {
    std::shared_ptr<std::string> str;
    std::shared_ptr<int64_t> sint;
    std::shared_ptr<uint64_t> uint;
    std::shared_ptr<double> d;

    U();
    ~U();

    U(std::string);
    U(int64_t);
    U(uint64_t);
    U(double);
  } u;

  TypeE type_enum;
};

class Connection {
 public:
  /// There can only be 1 valid Connection instance at a time.
  /// The private static timed_mutex ensures this.
  static std::optional<Connection> connect_msql(std::string addr, uint16_t port,
                                                std::string user,
                                                std::string pass,
                                                std::string dbname);

  Connection();
  ~Connection();

  // No copy
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  // Allow move
  Connection(Connection &&);
  Connection &operator=(Connection &&);

  bool is_valid() const;

  // Checks that the connection is valid with a ping.
  // If this fails, the fd is closed, the mutex is unlocked, and this
  // Connection becomes no longer valid.
  bool ping_check();

  // No value on failure. Vector on success. Non-empty vector if there are
  // results.
  using StmtRet = std::optional<std::vector<std::vector<Value> > >;
  StmtRet execute_stmt(const std::string &stmt, std::vector<Value> bind_params);

 private:
  // Destructor should unlock this.
  static std::timed_mutex m;
  // 0 - invalid connection if set.
  std::bitset<32> flags;
  int fd;
  uint32_t connection_id;

  Connection(int fd, uint32_t connection_id);
  void close_stmt(uint32_t stmt_id);
};

// Copies "data" into the returned packet struct(s).
std::vector<Packet> create_packets(uint8_t *data, size_t data_size,
                                   uint8_t *seq);

// Serializes Packets.
std::vector<PMA_HELPER::BinaryPart> packets_to_parts(
    const std::vector<Packet> &);

std::array<uint8_t, 20> msql_native_auth_resp(std::vector<uint8_t> seed,
                                              std::string pass);

// 0 on success, 1 if error. Second integer is bytes read.
std::tuple<int, size_t> handle_ok_pkt(uint8_t *data, size_t size);

void print_error_pkt(uint8_t *data, size_t size);

// Integer value and bytes read.
// Bytes read is 0 on error. Value is 0 on null.
std::tuple<uint64_t, uint_fast8_t> parse_len_enc_int(uint8_t *data);

// Returns server_caps_1, server_caps_2, server_caps_3, seed, auth_plugin_name,
// and connection_id.
std::optional<std::tuple<uint16_t, uint16_t, uint32_t, std::vector<uint8_t>,
                         std::string, uint32_t> >
parse_init_handshake_pkt(uint8_t *data, size_t size);

// Returns err int, stmt id.
// err int is 0 on success, 1 on error.
std::optional<std::tuple<int, uint32_t> > parse_prepare_resp_pkt(uint8_t *buf,
                                                                 size_t size);

// Returns column count.
std::optional<uint64_t> parse_column_count_pkt(uint8_t *buf, size_t size);

// Returns 0 on success. Updates field_types vec.
int parse_col_type_pkt(uint8_t *buf, size_t size,
                       std::vector<uint8_t> &field_types,
                       std::vector<uint16_t> &field_details);

// Returns 0 on success.
int parse_row_pkt(uint8_t *buf, size_t size,
                  const std::vector<uint8_t> &field_types,
                  const std::vector<uint16_t> &field_details,
                  std::vector<Value> *out);

void init_db(Connection &c);

std::optional<uint64_t> get_next_seq_id(Connection &c);

std::optional<bool> has_challenge_factor_id(Connection &c, std::string hash);

std::optional<bool> set_challenge_factor(Connection &c, std::string ip,
                                         std::string hash, uint16_t port,
                                         std::string factors_hash);

std::optional<uint16_t> get_id_to_port_port(Connection &c, std::string id);

std::optional<std::tuple<bool, uint16_t> > validate_client(
    Connection &c, uint64_t chall_factors_timeout, std::string id,
    std::string factors_hash, std::string client_ip);

std::optional<bool> client_is_allowed(Connection &c, std::string ip,
                                      uint16_t port,
                                      uint64_t allowed_ips_timeout);

std::optional<std::string> init_id_to_port(Connection &c, uint16_t port,
                                           uint64_t id_to_port_timeout);

struct Conf {
  std::string addr;
  std::string user;
  std::string pass;
  std::string db;
  uint16_t port;
};

std::optional<Conf> parse_conf_file(std::string msql_conf_path);

}  // Namespace PMA_MSQL

#endif
