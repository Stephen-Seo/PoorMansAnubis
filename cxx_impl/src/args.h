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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_ARGS_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_ARGS_H_

// Standard library includes
#include <bitset>
#include <cstdint>
#include <deque>
#include <string>
#include <tuple>
#include <unordered_map>

constexpr uint32_t DEFAULT_FACTORS_DIGITS = 5000;
constexpr uint32_t DEFAULT_JSON_MAX_SIZE = 10000;
constexpr uint32_t ALLOWED_IP_TIMEOUT_MINUTES = 60;
constexpr uint32_t CHALLENGE_FACTORS_TIMEOUT_MINUTES = 3;

namespace PMA_ARGS {

using AddrPort = std::tuple<std::string, uint16_t>;

struct Args {
  Args(int argc, char **argv);

  // Allow copy
  Args(const Args &other) = default;
  Args &operator=(const Args &other) = default;

  // Allow move
  Args(Args &&other) = default;
  Args &operator=(Args &&other) = default;

  uint64_t factors;
  std::string default_dest_url;
  std::deque<AddrPort> addr_ports;
  std::unordered_map<uint16_t, std::string> port_to_dest_urls;
  // 0 - enable trusting "x-real-ip" header
  // 1 - enable "override-dest-url" header
  // 2 - failed to parse args
  // 3 - potentially dangerous flags enabled
  std::bitset<32> flags;
  std::string api_url;
  std::string js_factors_url;
  std::string sqlite_path;
  uint32_t challenge_timeout;
  uint32_t allowed_timeout;
};

}  // namespace PMA_ARGS

#endif
