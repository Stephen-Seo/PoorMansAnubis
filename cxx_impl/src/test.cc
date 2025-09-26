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

// standard library includes
#include <atomic>
#include <print>

// local includes
#include "helpers.h"
#include "http.h"

#define ASSERT_TRUE(x)                                                        \
  test_count.fetch_add(1);                                                    \
  if (!(x)) {                                                                 \
    std::println(stderr, "ERROR: \"{}\" failed assert at {}!", #x, __LINE__); \
    std::println("{} out of {} tests succeeded", test_succeeded.load(),       \
                 test_count.load());                                          \
    return 1;                                                                 \
  } else {                                                                    \
    test_succeeded.fetch_add(1);                                              \
  }

#define CHECK_TRUE(x)                                                         \
  test_count.fetch_add(1);                                                    \
  if (!(x)) {                                                                 \
    std::println(stderr, "ERROR: \"{}\" failed assert at {}!", #x, __LINE__); \
  } else {                                                                    \
    test_succeeded.fetch_add(1);                                              \
  }

std::atomic_uint64_t test_count(0);
std::atomic_uint64_t test_succeeded(0);

int main() {
  // test raw_to_hexadecimal
  {
    std::array<uint8_t, 3> chars{0x12, 0x34, 0x56};
    std::string result = PMA_HELPER::raw_to_hexadecimal<3>(chars);
    ASSERT_TRUE(result == "123456");
  }

  // test str_to_ipv6_addr
  {
    std::array<uint8_t, 16> ipv6 =
        PMA_HTTP::str_to_ipv6_addr("1234:123:12:1::abcd");
    CHECK_TRUE(ipv6.at(0) == 0x12);
    CHECK_TRUE(ipv6.at(1) == 0x34);
    CHECK_TRUE(ipv6.at(2) == 1);
    CHECK_TRUE(ipv6.at(3) == 0x23);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 0x12);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 1);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 0);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 0);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0);
    CHECK_TRUE(ipv6.at(14) == 0xab);
    CHECK_TRUE(ipv6.at(15) == 0xcd);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("5678:9:12:123::fedc:ba:c");
    CHECK_TRUE(ipv6.at(0) == 0x56);
    CHECK_TRUE(ipv6.at(1) == 0x78);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0x9);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 0x12);
    CHECK_TRUE(ipv6.at(6) == 1);
    CHECK_TRUE(ipv6.at(7) == 0x23);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 0);
    CHECK_TRUE(ipv6.at(10) == 0xfe);
    CHECK_TRUE(ipv6.at(11) == 0xdc);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0xba);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 0xc);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("::1467:235:89:a");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 0);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 0);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 0);
    CHECK_TRUE(ipv6.at(8) == 0x14);
    CHECK_TRUE(ipv6.at(9) == 0x67);
    CHECK_TRUE(ipv6.at(10) == 0x2);
    CHECK_TRUE(ipv6.at(11) == 0x35);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0x89);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 0xa);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("12:3:456:abc:defa::");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 0x12);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0x3);
    CHECK_TRUE(ipv6.at(4) == 4);
    CHECK_TRUE(ipv6.at(5) == 0x56);
    CHECK_TRUE(ipv6.at(6) == 0xa);
    CHECK_TRUE(ipv6.at(7) == 0xbc);
    CHECK_TRUE(ipv6.at(8) == 0xde);
    CHECK_TRUE(ipv6.at(9) == 0xfa);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 0);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 0);

    ipv6 =
        PMA_HTTP::str_to_ipv6_addr("1234:5678:9abc:def0:1234:5678:9abc:def0");
    CHECK_TRUE(ipv6.at(0) == 0x12);
    CHECK_TRUE(ipv6.at(1) == 0x34);
    CHECK_TRUE(ipv6.at(2) == 0x56);
    CHECK_TRUE(ipv6.at(3) == 0x78);
    CHECK_TRUE(ipv6.at(4) == 0x9a);
    CHECK_TRUE(ipv6.at(5) == 0xbc);
    CHECK_TRUE(ipv6.at(6) == 0xde);
    CHECK_TRUE(ipv6.at(7) == 0xf0);
    CHECK_TRUE(ipv6.at(8) == 0x12);
    CHECK_TRUE(ipv6.at(9) == 0x34);
    CHECK_TRUE(ipv6.at(10) == 0x56);
    CHECK_TRUE(ipv6.at(11) == 0x78);
    CHECK_TRUE(ipv6.at(12) == 0x9a);
    CHECK_TRUE(ipv6.at(13) == 0xbc);
    CHECK_TRUE(ipv6.at(14) == 0xde);
    CHECK_TRUE(ipv6.at(15) == 0xf0);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("1:12:345:6789:abc:de:f:2");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 1);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0x12);
    CHECK_TRUE(ipv6.at(4) == 3);
    CHECK_TRUE(ipv6.at(5) == 0x45);
    CHECK_TRUE(ipv6.at(6) == 0x67);
    CHECK_TRUE(ipv6.at(7) == 0x89);
    CHECK_TRUE(ipv6.at(8) == 0xa);
    CHECK_TRUE(ipv6.at(9) == 0xbc);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 0xde);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0xf);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 2);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("0:1234:0:3a5:9:0:45:1");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 0);
    CHECK_TRUE(ipv6.at(2) == 0x12);
    CHECK_TRUE(ipv6.at(3) == 0x34);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 0);
    CHECK_TRUE(ipv6.at(6) == 0x3);
    CHECK_TRUE(ipv6.at(7) == 0xa5);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 9);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 0);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0x45);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 1);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("::");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 0);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 0);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 0);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 0);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 0);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 0);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("::1");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 0);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 0);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 0);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 0);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 0);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 1);

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr(":::");
      CHECK_TRUE(!"Should have failed to parse \":::\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:23:::456");
      CHECK_TRUE(!"Should have failed to parse \"1:23:::456\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7:8");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 1);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 2);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 3);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 4);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 5);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 6);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 7);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 8);

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7:8:9");
      CHECK_TRUE(!"Should have failed to parse \"1:2:3:4:5:6:7:8:9\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1::2::3:4");
      CHECK_TRUE(!"Should have failed to parse \"1::2::3:4\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:::23::2:1:5");
      CHECK_TRUE(!"Should have failed to parse \"1:::23::2:1:5\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    ipv6 = PMA_HTTP::str_to_ipv6_addr("1::3:4:5:6:7:8");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 1);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 3);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 4);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 5);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 6);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 7);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 8);

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1::2:3:4:5:6:7:8");
      CHECK_TRUE(!"Should have failed to parse \"1::2:3:4:5:6:7:8\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1::2:3:4:5:6:7:8:9");
      CHECK_TRUE(!"Should have failed to parse \"1::2:3:4:5:6:7:8:9\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1::2:3:4:5:6:7:8:9:10");
      CHECK_TRUE(
          !"Should have failed to parse \"1::2:3:4:5:6:7:8:9:10\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5::6");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 1);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 2);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 3);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 4);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 5);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 0);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 6);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6::7");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 1);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 2);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 3);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 4);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 5);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 6);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 7);

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7::8");
      CHECK_TRUE(!"Should have failed to parse \"1:2:3:4:5:6:7::8\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7:8::9");
      CHECK_TRUE(!"Should have failed to parse \"1:2:3:4:5:6:7:8::9\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7:8:9::10");
      CHECK_TRUE(
          !"Should have failed to parse \"1:2:3:4:5:6:7:8:9::10\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    ipv6 = PMA_HTTP::str_to_ipv6_addr("::1:2:3:4:5:6");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 0);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 0);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 1);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 2);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 3);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 4);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 5);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 6);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("::1:2:3:4:5:6:7");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 0);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 1);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 2);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 3);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 4);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 5);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 6);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 7);

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("::1:2:3:4:5:6:7:8");
      CHECK_TRUE(!"Should have failed to parse \"::1:2:3:4:5:6:7:8\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("::1:2:3:4:5:6:7:8:9");
      CHECK_TRUE(
          !"Should have failed to parse \"::1:2:3:4:5:6:7:8:9\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6::");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 1);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 2);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 3);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 4);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 5);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 6);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 0);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 0);

    ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7::");
    CHECK_TRUE(ipv6.at(0) == 0);
    CHECK_TRUE(ipv6.at(1) == 1);
    CHECK_TRUE(ipv6.at(2) == 0);
    CHECK_TRUE(ipv6.at(3) == 2);
    CHECK_TRUE(ipv6.at(4) == 0);
    CHECK_TRUE(ipv6.at(5) == 3);
    CHECK_TRUE(ipv6.at(6) == 0);
    CHECK_TRUE(ipv6.at(7) == 4);
    CHECK_TRUE(ipv6.at(8) == 0);
    CHECK_TRUE(ipv6.at(9) == 5);
    CHECK_TRUE(ipv6.at(10) == 0);
    CHECK_TRUE(ipv6.at(11) == 6);
    CHECK_TRUE(ipv6.at(12) == 0);
    CHECK_TRUE(ipv6.at(13) == 7);
    CHECK_TRUE(ipv6.at(14) == 0);
    CHECK_TRUE(ipv6.at(15) == 0);

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7:8::");
      CHECK_TRUE(!"Should have failed to parse \"1:2:3:4:5:6:7:8::\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("1:2:3:4:5:6:7:8:9::");
      CHECK_TRUE(
          !"Should have failed to parse \"1:2:3:4:5:6:7:8:9::\" as ipv6");
    } catch (const std::exception &e) {
      CHECK_TRUE("Successfully caught expected exception");
    }
  }

  std::println("{} out of {} tests succeeded", test_succeeded.load(),
               test_count.load());
  return test_succeeded.load() == test_count.load() ? 0 : 1;
}
