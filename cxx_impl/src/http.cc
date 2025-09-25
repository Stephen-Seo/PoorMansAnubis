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
#include <sys/socket.h>
#include <unistd.h>

// Standard library includes
#include <cstring>
#include <format>
#include <print>
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
    default:
      return "UnknownError";
  }
}

std::array<uint8_t, 16> PMA_HTTP::str_to_ipv6_addr(std::string addr) {
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
        break;
      }
    } else {
      colon_count = 0;
    }
  }

  if (has_double_colon) {
    // Populate left side.
    uint64_t a_idx = 0;
    uint8_t byte = 0;
    uint_fast8_t byte_count = 0;
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
          byte_count = 0;
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
              throw std::logic_error("Failed to parse");
            }

            ++a_idx;
            ipv6_addr.at(a_idx++) = byte;
            break;
          case 2:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
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
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          case 4:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          default:
            throw std::logic_error(
                std::format("Failed to parse, count is {}", segment_count));
        }

        segment_count = 0;
        checking_for_segment = true;
        ++idx;
        if (a_idx >= ipv6_addr.size() || idx >= addr.size() ||
            idx >= double_colon_idx) {
          break;
        } else if (addr.at(idx) != ':') {
          throw std::logic_error("Failed to parse");
        }
      }
      ++idx;
    }

    // Populate right side.
    a_idx = ipv6_addr.size() - 1;
    byte = 0;
    byte_count = 0;
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
          byte_count = 0;
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
              throw std::logic_error("Failed to parse");
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
              throw std::logic_error("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
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
              throw std::logic_error("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
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
              throw std::logic_error("Failed to parse");
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
              throw std::logic_error("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
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
              throw std::logic_error("Failed to parse");
            }

            --idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx--) = byte;
            break;
          default:
            throw std::logic_error(
                std::format("Failed to parse, count is {}", segment_count));
        }

        segment_count = 0;
        checking_for_segment = true;
        --idx;
        if (a_idx >= ipv6_addr.size() || idx < double_colon_idx + 2 ||
            idx >= addr.size()) {
          break;
        } else if (addr.at(idx) != ':') {
          throw std::logic_error("Failed to parse");
        }
      }
      --idx;
    }
  } else {
    // Populate full ipv6 address.
    uint64_t a_idx = 0;
    uint8_t byte = 0;
    uint_fast8_t byte_count = 0;
    uint_fast8_t segment_count = 0;
    bool checking_for_segment = true;
    uint64_t idx = 0;
    while (true) {
      if (idx >= addr.size() || addr.at(idx) == ':') {
        if (checking_for_segment) {
          idx -= segment_count;
          checking_for_segment = false;
          byte = 0;
          byte_count = 0;
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
              throw std::logic_error("Failed to parse");
            }

            ++a_idx;
            ipv6_addr.at(a_idx++) = byte;
            break;
          case 2:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
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
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          case 4:
            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte = ((addr.at(idx) - '0') & 0xF) << 4;
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte = ((addr.at(idx) - 'a' + 10) & 0xF) << 4;
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte = ((addr.at(idx) - 'A' + 10) & 0xF) << 4;
            } else {
              throw std::logic_error("Failed to parse");
            }

            ++idx;

            if (addr.at(idx) >= '0' && addr.at(idx) <= '9') {
              byte |= ((addr.at(idx) - '0') & 0xF);
            } else if (addr.at(idx) >= 'a' && addr.at(idx) <= 'f') {
              byte |= ((addr.at(idx) - 'a' + 10) & 0xF);
            } else if (addr.at(idx) >= 'A' && addr.at(idx) <= 'F') {
              byte |= ((addr.at(idx) - 'A' + 10) & 0xF);
            } else {
              throw std::logic_error("Failed to parse");
            }

            ipv6_addr.at(a_idx++) = byte;
            break;
          default:
            throw std::logic_error(
                std::format("Failed to parse, count is {}", segment_count));
        }

        segment_count = 0;
        checking_for_segment = true;
        ++idx;
        if (a_idx >= ipv6_addr.size() || idx >= addr.size()) {
          break;
        } else if (addr.at(idx) != ':') {
          throw std::logic_error("Failed to parse");
        }
      }
      ++idx;
    }
  }

  return ipv6_addr;
}

std::tuple<PMA_HTTP::ErrorT, std::string, int> PMA_HTTP::get_ipv6_socket(
    std::string addr, uint16_t port) {
  int socket_fd = socket(AF_INET6, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
            std::format("Failed to create socket, errno {}", errno), 0};
  }

  // bind to [::], with port
  {
    struct sockaddr_in6 sain6;
    sain6.sin6_family = AF_INET6;
    sain6.sin6_port = PMA_HELPER::be_swap_u16(port);
    sain6.sin6_flowinfo = 0;
    std::memset(sain6.sin6_addr.s6_addr, 0, 16);
    sain6.sin6_scope_id = 0;

    int ret = bind(socket_fd, reinterpret_cast<const sockaddr*>(&sain6),
                   sizeof(struct sockaddr_in6));
    if (ret != 0) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
              std::format("Failed to bind socket, errno {}", errno), 0};
    }
  }

  // set enable listen
  {
    int ret = listen(socket_fd, SOCKET_BACKLOG_SIZE);
    if (ret == -1) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV6_SOCKET,
              std::format("Failed to set socket to listen, errno {}", errno),
              0};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}

std::tuple<PMA_HTTP::ErrorT, std::string, int> PMA_HTTP::get_ipv4_socket(
    std::string addr, uint16_t port) {
  int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 6);
  if (socket_fd == -1) {
    return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
            std::format("Failed to create socket, errno {}", errno), 0};
  }

  // bind to 0.0.0.0, with port
  {
    struct sockaddr_in sain;
    sain.sin_family = AF_INET;
    sain.sin_port = PMA_HELPER::be_swap_u16(port);
    sain.sin_addr.s_addr = 0;

    int ret = bind(socket_fd, reinterpret_cast<const sockaddr*>(&sain),
                   sizeof(struct sockaddr_in));
    if (ret != 0) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
              std::format("Failed to bind socket, errno {}", errno), 0};
    }
  }

  // set enable listen
  {
    int ret = listen(socket_fd, SOCKET_BACKLOG_SIZE);
    if (ret == -1) {
      close(socket_fd);
      return {ErrorT::FAILED_TO_GET_IPV4_SOCKET,
              std::format("Failed to set socket to listen, errno {}", errno),
              0};
    }
  }

  return {ErrorT::SUCCESS, {}, socket_fd};
}
