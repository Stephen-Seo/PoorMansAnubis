// ISC License
//
// Copyright (c) 2025-2026 Stephen Seo
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
#include <unordered_map>

namespace PMA_HTTP {

enum class ErrorT {
  SUCCESS,
  FAILED_TO_GET_IPV6_SOCKET,
  FAILED_TO_GET_IPV4_SOCKET,
  FAILED_TO_CONNECT_IPV6_SOCKET,
  FAILED_TO_CONNECT_IPV4_SOCKET,
  FAILED_TO_PARSE_IPV6,
  FAILED_TO_PARSE_IPV4,
  NOT_GET_NOR_POST_REQ,
  INVALID_STATE,
  FAILED_TO_PARSE_JSON
};

std::string error_t_to_str(ErrorT err_enum);

/// May throw an exception when given an invalid string.
std::tuple<std::array<uint8_t, 16>, uint32_t> str_to_ipv6_addr(
    const std::string &addr) noexcept(false);
std::string ipv6_addr_to_str(const std::array<uint8_t, 16> &ipv6) noexcept;

/// May throw an exception when given an invalid string.
/// The returned value should be in big-endian.
uint32_t str_to_ipv4_addr(const std::string &addr) noexcept(false);
/// The given parameter should be in big-endian.
std::string ipv4_addr_to_str(uint32_t ipv4) noexcept;

std::tuple<ErrorT, std::string, int> get_ipv6_socket_server(std::string addr,
                                                            uint16_t port);
std::tuple<ErrorT, std::string, int> get_ipv4_socket_server(std::string addr,
                                                            uint16_t port);

std::tuple<ErrorT, std::string, int> connect_ipv6_socket_client(
    std::string server_addr, std::string client_addr, uint16_t port);
std::tuple<ErrorT, std::string, int> connect_ipv4_socket_client(
    std::string server_addr, std::string client_addr, uint16_t port);

struct Request {
  std::unordered_map<std::string, std::string> queries;
  std::unordered_map<std::string, std::string> headers;
  std::string url_or_err_msg;
  std::string full_url;
  std::string body;
  std::string method;
  ErrorT error_enum;

  static Request from_error(ErrorT, std::string);
};

/// Parses request to get url, query params, and headers.
/// On error, string is err message. On SUCCESS, string is request url
/// First map is key/value pairs of query parameters
/// Second map is key/value headers
Request handle_request_parse(std::string req);

std::tuple<ErrorT, std::unordered_map<std::string, std::string> >
    parse_simple_json(std::string);

}  // namespace PMA_HTTP

#endif
