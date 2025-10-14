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

#include "http.h"

// Unix includes
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// Standard library includes
#include <cstring>
#include <format>
#include <stdexcept>

// Local includes
#include "helpers.h"

std::string PMA_HTTP::error_t_to_str(PMA_HTTP::ErrorT err_enum) {
  switch (err_enum) {
    case ErrorT::SUCCESS:
      return "SUCCESS";
    case ErrorT::FAILED_TO_GET_IPV6_SOCKET:
      return "FailedToGetIPV6Socket";
    case ErrorT::FAILED_TO_GET_IPV4_SOCKET:
      return "FailedToGetIPV4Socket";
    case ErrorT::FAILED_TO_CONNECT_IPV6_SOCKET:
      return "FailedToConnectIPV6Socket";
    case ErrorT::FAILED_TO_CONNECT_IPV4_SOCKET:
      return "FailedToConnectIPV4Socket";
    case ErrorT::FAILED_TO_PARSE_IPV6:
      return "FailedToParseIPV6";
    case ErrorT::FAILED_TO_PARSE_IPV4:
      return "FailedToParseIPV4";
    default:
      return "UnknownError";
  }
}

std::array<uint8_t, 16> PMA_HTTP::str_to_ipv6_addr(
    const std::string &addr) noexcept(false) {
  std::array<uint8_t, 16> ipv6_addr;
  // Memset to zero first.
  std::memset(ipv6_addr.data(), 0, 16);

  // Check for [::] case
  if (addr == "::" || addr == "[::]") {
    return ipv6_addr;
  }

  bool has_double_colon = false;

  // first check for double_colon
  int colon_count = 0;
  uint64_t double_colon_idx = 0;
  for (uint64_t idx = 0; idx < addr.size(); ++idx) {
    if (addr.at(idx) == ':') {
      ++colon_count;
      if (colon_count == 2) {
        has_double_colon = true;
        double_colon_idx = idx - 1;
      } else if (colon_count > 2) {
        throw std::invalid_argument("Too many consecutive colons");
      }
    } else {
      colon_count = 0;
    }
  }

  if (has_double_colon) {
    // Validate number of segments.
    {
      int segment_count = 1;
      bool reached_colon = false;
      bool reached_value = false;
      for (uint64_t idx = 0; idx < addr.size(); ++idx) {
        if (addr.at(idx) == ':') {
          if (!reached_colon && reached_value) {
            ++segment_count;
            reached_colon = true;
          }
        } else {
          reached_colon = false;
          reached_value = true;
        }
      }

      if (addr.substr(addr.size() - 2, 2) == "::") {
        --segment_count;
      }

      if (segment_count > 7) {
        throw std::invalid_argument(
            "Invalid number of segments for full ipv6 addr");
      }
    }

    // Populate left side.
    uint64_t a_idx = 0;
    uint8_t byte = 0;
    uint_fast8_t segment_count = 0;
    bool checking_for_segment = true;
    uint64_t idx = 0;
    while (true) {
      if (double_colon_idx == 0) {
        break;
      } else if (addr.at(idx) == ':') {
        if (checking_for_segment) {
          idx -= segment_count;
          checking_for_segment = false;
          byte = 0;
          continue;
        }
      } else if (checking_for_segment) {
        ++segment_count;
      } else {
        switch (segment_count) {
          case 1:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++a_idx;
            ipv6_addr.at(a_idx++) = byte;
            break;
          case 2:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++a_idx;
            ipv6_addr.at(a_idx++) = byte;
            break;
          case 3:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          case 4:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          default:
            throw std::invalid_argument(
                std::format("Failed to parse, count is {}", segment_count));
        }

        segment_count = 0;
        checking_for_segment = true;
        ++idx;
        if (a_idx >= ipv6_addr.size() || idx >= addr.size() ||
            idx >= double_colon_idx) {
          break;
        } else if (addr.at(idx) != ':') {
          throw std::invalid_argument("Failed to parse");
        }
      }
      ++idx;
    }

    // Populate right side.
    a_idx = ipv6_addr.size() - 1;
    byte = 0;
    segment_count = 0;
    checking_for_segment = true;
    idx = addr.size() - 1;
    while (true) {
      if (double_colon_idx + 2 == addr.size()) {
        break;
      } else if (addr.at(idx) == ':') {
        if (checking_for_segment) {
          idx += segment_count;
          checking_for_segment = false;
          byte = 0;
          continue;
        }
      } else if (checking_for_segment) {
        ++segment_count;
      } else {
        switch (segment_count) {
          case 1:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx--) = byte;
            --a_idx;
            break;
          case 2:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx--) = byte;
            --a_idx;
            break;
          case 3:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx--) = byte;

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx--) = byte;
            break;
          case 4:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx--) = byte;

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |=
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx--) = byte;
            break;
          default:
            throw std::invalid_argument(
                std::format("Failed to parse, count is {}", segment_count));
        }

        segment_count = 0;
        checking_for_segment = true;
        --idx;
        if (a_idx >= ipv6_addr.size() || idx < double_colon_idx + 2 ||
            idx >= addr.size()) {
          break;
        } else if (addr.at(idx) != ':') {
          throw std::invalid_argument("Failed to parse");
        }
      }
      --idx;
    }
  } else {
    // Validate number of segments.
    {
      int segment_count = 1;
      for (uint64_t idx = 0; idx < addr.size(); ++idx) {
        if (addr.at(idx) == ':') {
          ++segment_count;
        }
      }
      if (segment_count > 8) {
        throw std::invalid_argument(
            "Invalid number of segments for full ipv6 addr");
      }
    }

    // Populate full ipv6 address.
    uint64_t a_idx = 0;
    uint8_t byte = 0;
    uint_fast8_t segment_count = 0;
    bool checking_for_segment = true;
    uint64_t idx = 0;
    while (true) {
      if (idx >= addr.size() || addr.at(idx) == ':') {
        if (checking_for_segment) {
          idx -= segment_count;
          checking_for_segment = false;
          byte = 0;
          continue;
        }
      } else if (checking_for_segment) {
        ++segment_count;
      } else {
        switch (segment_count) {
          case 1:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++a_idx;
            ipv6_addr.at(a_idx++) = byte;
            break;
          case 2:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++a_idx;
            ipv6_addr.at(a_idx++) = byte;
            break;
          case 3:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          case 4:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = static_cast<uint8_t>(((addr.at(idx) - '0') & 0xF) << 4);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'a' + 10) & 0xF) << 4);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte =
                  static_cast<uint8_t>(((addr.at(idx) - 'A' + 10) & 0xF) << 4);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= static_cast<uint8_t>((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= static_cast<uint8_t>((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::invalid_argument("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          default:
            throw std::invalid_argument(
                std::format("Failed to parse, count is {}", segment_count));
        }

        segment_count = 0;
        checking_for_segment = true;
        ++idx;
        if (a_idx >= ipv6_addr.size() || idx >= addr.size()) {
          break;
        } else if (addr.at(idx) != ':') {
          throw std::invalid_argument("Failed to parse");
        }
      }
      ++idx;
    }
  }

  return ipv6_addr;
}

std::string PMA_HTTP::ipv6_addr_to_str(
    const std::array<uint8_t, 16> &ipv6) noexcept {
  // Check for a region of zeroes
  int zidx_0 = -1;
  int zidx_1 = -1;
  int hold = -2;
  for (int idx = 0; idx < static_cast<int>(ipv6.size()); ++idx) {
    if (idx % 2 == 0) {
      if (ipv6.at(idx) == 0) {
        hold = idx;
      } else {
        hold = -2;
      }
    }

    if (hold + 1 == idx && ipv6.at(idx) == 0) {
      if (zidx_0 == -1) {
        zidx_0 = hold;
        zidx_1 = idx + 1;
      } else if (zidx_1 + 1 == idx) {
        zidx_1 = idx + 1;
      }
    }
  }

  if (zidx_0 == 0 && zidx_1 == static_cast<int>(ipv6.size())) {
    return "::";
  }

  // left side
  std::string addr_str;
  bool prev_has_colon = true;
  bool prev_had_value = false;
  for (int idx = 0; idx < zidx_0; ++idx) {
    std::string byte_str = PMA_HELPER::byte_to_hex(ipv6.at(idx));
    if (byte_str != "0" || idx % 2 == 1) {
      if (prev_had_value && byte_str.size() == 1) {
        addr_str.push_back('0');
      }
      addr_str.append(byte_str);
      if (idx % 2 == 0) {
        prev_had_value = true;
      } else {
        prev_had_value = false;
      }
    } else {
      prev_had_value = false;
    }

    if (!prev_has_colon && idx + 1 != static_cast<int>(ipv6.size()) &&
        (zidx_0 < 0 || zidx_0 != idx + 1)) {
      addr_str.push_back(':');
    }

    prev_has_colon = idx % 2 == 1;
  }

  // middle
  if (zidx_0 >= 0) {
    addr_str.push_back(':');
    addr_str.push_back(':');
  }

  // right side
  prev_has_colon = true;
  prev_had_value = false;
  for (int idx = zidx_1 > 0 ? zidx_1 : 0; idx < static_cast<int>(ipv6.size());
       ++idx) {
    std::string byte_str = PMA_HELPER::byte_to_hex(ipv6.at(idx));
    if (byte_str != "0" || idx % 2 == 1) {
      if (prev_had_value && byte_str.size() == 1) {
        addr_str.push_back('0');
      }
      addr_str.append(byte_str);
      if (idx % 2 == 0) {
        prev_had_value = true;
      } else {
        prev_had_value = false;
      }
    } else {
      prev_had_value = false;
    }

    if (!prev_has_colon && idx + 1 != static_cast<int>(ipv6.size())) {
      addr_str.push_back(':');
    }

    prev_has_colon = idx % 2 == 1;
  }

  return addr_str;
}

uint32_t PMA_HTTP::str_to_ipv4_addr(const std::string &addr) noexcept(false) {
  union {
    uint32_t u32;
    std::array<uint8_t, 4> u8_arr;
  } addr_u;

  addr_u.u32 = 0;

  int addr_idx = 4;
  int value = 0;
  for (uint64_t idx = 0; idx < addr.size(); ++idx) {
    if (addr[idx] >= '0' && addr[idx] <= '9') {
      value = value * 10 + static_cast<int>(addr[idx] - '0');
      if (value > 255) {
        throw std::invalid_argument(
            "Failed to parse, segment greater than 255");
      }
    } else if (addr[idx] == '.') {
      addr_u.u8_arr.at(--addr_idx) = static_cast<uint8_t>(value);
      value = 0;
    } else {
      throw std::invalid_argument("Failed to parse");
    }
  }
  addr_u.u8_arr.at(--addr_idx) = static_cast<uint8_t>(value);

  return PMA_HELPER::be_swap_u32(addr_u.u32);
}

std::string PMA_HTTP::ipv4_addr_to_str(uint32_t ipv4) noexcept {
  std::string ret;

  union {
    uint32_t u32;
    std::array<uint8_t, 4> u8_arr;
  } addr_u;

  addr_u.u32 = PMA_HELPER::be_swap_u32(ipv4);

  for (int idx = 4; idx-- > 0;) {
    ret.append(std::format("{}", addr_u.u8_arr.at(idx)));
    if (idx > 0) {
      ret.push_back('.');
    }
  }

  return ret;
}

std::tuple<PMA_HTTP::ErrorT, std::string, int> PMA_HTTP::get_ipv6_socket_server(
    std::string addr, uint16_t port) {
  int socket_fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
            std::format("Failed to create socket, errno {}", errno), -1};
  }

  // bind to "addr", with port
  {
    std::array<uint8_t, 16> ipv6_addr;
    try {
      ipv6_addr = str_to_ipv6_addr(addr);
    } catch (const std::exception &e) {
      return {ErrorT::FAILED_TO_PARSE_IPV6, "Failed to parse ipv6 address", -1};
    }

    struct sockaddr_in6 sain6;
    sain6.sin6_family = AF_INET6;
    sain6.sin6_port = PMA_HELPER::be_swap_u16(port);
    sain6.sin6_flowinfo = 0;
    std::memcpy(sain6.sin6_addr.s6_addr, ipv6_addr.data(), 16);
    sain6.sin6_scope_id = 0;

    int ret = bind(socket_fd, reinterpret_cast<const sockaddr *>(&sain6),
                   sizeof(struct sockaddr_in6));
    if (ret != 0) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
              std::format("Failed to bind socket, errno {}", errno), -1};
    }
  }

  // set enable listen
  {
    int ret = listen(socket_fd, SOCKET_BACKLOG_SIZE);
    if (ret == -1) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
              std::format("Failed to set socket to listen, errno {}", errno),
              -1};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}

std::tuple<PMA_HTTP::ErrorT, std::string, int> PMA_HTTP::get_ipv4_socket_server(
    std::string addr, uint16_t port) {
  int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
            std::format("Failed to create socket, errno {}", errno), -1};
  }

  // bind to "addr", with port
  {
    struct sockaddr_in sain;
    sain.sin_family = AF_INET;
    sain.sin_port = PMA_HELPER::be_swap_u16(port);
    try {
      sain.sin_addr.s_addr = str_to_ipv4_addr(addr);
    } catch (const std::exception &e) {
      return {ErrorT::FAILED_TO_PARSE_IPV4, "Failed to parse ipv4 address", -1};
    }

    int ret = bind(socket_fd, reinterpret_cast<const sockaddr *>(&sain),
                   sizeof(struct sockaddr_in));
    if (ret != 0) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
              std::format("Failed to bind socket, errno {}", errno), -1};
    }
  }

  // set enable listen
  {
    int ret = listen(socket_fd, SOCKET_BACKLOG_SIZE);
    if (ret == -1) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
              std::format("Failed to set socket to listen, errno {}", errno),
              -1};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}

std::tuple<PMA_HTTP::ErrorT, std::string, int>
PMA_HTTP::connect_ipv6_socket_client(std::string server_addr,
                                     std::string client_addr, uint16_t port) {
  int socket_fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
            std::format("Failed to create socket, errno {}", errno), -1};
  }

  struct sockaddr_in6 sain6;
  sain6.sin6_family = AF_INET6;
  sain6.sin6_port = 0;
  sain6.sin6_flowinfo = 0;
  std::array<uint8_t, 16> ipv6_addr = str_to_ipv6_addr(client_addr);
  std::memcpy(sain6.sin6_addr.s6_addr, ipv6_addr.data(), 16);
  sain6.sin6_scope_id = 0;

  int ret = bind(socket_fd, reinterpret_cast<const sockaddr *>(&sain6),
                 sizeof(struct sockaddr_in6));
  if (ret == -1) {
    close(socket_fd);
    return {ErrorT::FAILED_TO_CONNECT_IPV6_SOCKET,
            std::format("Failed to bind socket, errno {}", errno), -1};
  }

  sain6.sin6_port = PMA_HELPER::be_swap_u16(port);
  ipv6_addr = str_to_ipv6_addr(server_addr);
  std::memcpy(sain6.sin6_addr.s6_addr, ipv6_addr.data(), 16);

  ret = connect(socket_fd, reinterpret_cast<sockaddr *>(&sain6),
                sizeof(struct sockaddr_in6));
  if (ret == -1) {
    if (errno == EINPROGRESS) {
      struct pollfd pfd = {socket_fd, POLLOUT, 0};
      ret = poll(&pfd, 1, /* millis */ 1000);
      if (ret == -1) {
        if (socket_fd != -1) {
          close(socket_fd);
        }
        return {ErrorT::FAILED_TO_CONNECT_IPV6_SOCKET,
                std::format("Failed to poll socket, errno {}", errno), -1};
      }

      int so_error_val = 0;
      socklen_t val_len = sizeof(int);
      ret =
          getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &so_error_val, &val_len);
      if (ret == -1) {
        if (socket_fd != -1) {
          close(socket_fd);
        }
        return {ErrorT::FAILED_TO_CONNECT_IPV6_SOCKET,
                std::format("Failed to \"getsockopt\" socket, errno {}", errno),
                -1};
      } else if (so_error_val != 0) {
        if (socket_fd != -1) {
          close(socket_fd);
        }
        return {ErrorT::FAILED_TO_CONNECT_IPV6_SOCKET,
                std::format("\"getsockopt\" returned non-zero: {}, errno {}",
                            so_error_val, errno),
                -1};
      }
    } else {
      if (socket_fd != -1) {
        close(socket_fd);
      }
      return {ErrorT::FAILED_TO_CONNECT_IPV6_SOCKET,
              std::format("Failed to connect socket, errno {}", errno), -1};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}

std::tuple<PMA_HTTP::ErrorT, std::string, int>
PMA_HTTP::connect_ipv4_socket_client(std::string server_addr,
                                     std::string client_addr, uint16_t port) {
  int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
            std::format("Failed to create socket, errno {}", errno), -1};
  }

  struct sockaddr_in sain;
  sain.sin_family = AF_INET;
  sain.sin_port = 0;
  sain.sin_addr.s_addr = str_to_ipv4_addr(client_addr);

  int ret = bind(socket_fd, reinterpret_cast<const sockaddr *>(&sain),
                 sizeof(struct sockaddr_in));
  if (ret == -1) {
    close(socket_fd);
    return {ErrorT::FAILED_TO_CONNECT_IPV4_SOCKET,
            std::format("Failed to bind socket to addr {}, errno {}",
                        client_addr, errno),
            -1};
  }

  sain.sin_port = PMA_HELPER::be_swap_u16(port);
  sain.sin_addr.s_addr = str_to_ipv4_addr(server_addr);

  ret = connect(socket_fd, reinterpret_cast<sockaddr *>(&sain),
                sizeof(struct sockaddr_in));
  if (ret == -1) {
    if (errno == EINPROGRESS) {
      struct pollfd pfd = {socket_fd, POLLOUT, 0};
      ret = poll(&pfd, 1, /* millis */ 1000);
      if (ret == -1) {
        if (socket_fd != -1) {
          close(socket_fd);
        }
        return {ErrorT::FAILED_TO_CONNECT_IPV4_SOCKET,
                std::format("Failed to poll socket, errno {}", errno), -1};
      }

      int so_error_val = 0;
      socklen_t val_len = sizeof(int);
      ret =
          getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &so_error_val, &val_len);
      if (ret == -1) {
        if (socket_fd != -1) {
          close(socket_fd);
        }
        return {ErrorT::FAILED_TO_CONNECT_IPV4_SOCKET,
                std::format("Failed to \"getsockopt\" socket, errno {}", errno),
                -1};
      } else if (so_error_val != 0) {
        if (socket_fd != -1) {
          close(socket_fd);
        }
        return {ErrorT::FAILED_TO_CONNECT_IPV4_SOCKET,
                std::format("\"getsockopt\" returned non-zero: {}, errno {}",
                            so_error_val, errno),
                -1};
      }
    } else {
      if (socket_fd != -1) {
        close(socket_fd);
      }
      return {ErrorT::FAILED_TO_CONNECT_IPV4_SOCKET,
              std::format("Failed to connect socket, errno {}", errno), -1};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}
