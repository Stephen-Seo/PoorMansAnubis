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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_HTTP_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_HTTP_H_

// Standard library includes
#include <cstdint>
#include <string>
#include <tuple>

namespace PMA_HTTP {
constexpr int SOCKET_BACKLOG_SIZE = 32;

enum class ErrorT {
  SUCCESS,
  FAILED_TO_GET_IPV6_SOCKET,
  FAILED_TO_GET_IPV4_SOCKET
};

std::string error_t_to_str(ErrorT err_enum);

std::array<uint8_t, 16> str_to_ipv6_addr(const std::string &addr);

// The returned value should be in big-endian.
uint32_t str_to_ipv4_addr(const std::string &addr);

std::tuple<ErrorT, std::string, int> get_ipv6_socket(std::string addr,
                                                     uint16_t port);
std::tuple<ErrorT, std::string, int> get_ipv4_socket(std::string addr,
                                                     uint16_t port);
}  // namespace PMA_HTTP

#endif
