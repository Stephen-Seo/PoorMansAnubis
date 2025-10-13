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
#include <cstring>
#include <print>
#include <random>

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

  // test endian_swap_*
  {
    uint16_t u16 = 0x12ab;
    u16 = PMA_HELPER::endian_swap_u16(u16);
    ASSERT_TRUE(u16 == 0xab12);
    u16 = PMA_HELPER::endian_swap_u16(u16);
    ASSERT_TRUE(u16 == 0x12ab);

    uint32_t u32 = 0x1234abcd;
    u32 = PMA_HELPER::endian_swap_u32(u32);
    ASSERT_TRUE(u32 == 0xcdab3412);
    u32 = PMA_HELPER::endian_swap_u32(u32);
    ASSERT_TRUE(u32 == 0x1234abcd);

    uint64_t u64 = 0x12345678abcdefdd;
    u64 = PMA_HELPER::endian_swap_u64(u64);
    ASSERT_TRUE(u64 == 0xddefcdab78563412);
    u64 = PMA_HELPER::endian_swap_u64(u64);
    ASSERT_TRUE(u64 == 0x12345678abcdefdd);
  }

  // test byte_to_hex
  {
    CHECK_TRUE(PMA_HELPER::byte_to_hex(0x4A) == "4A");
    CHECK_TRUE(PMA_HELPER::byte_to_hex(0xd4) == "D4");
    CHECK_TRUE(PMA_HELPER::byte_to_hex(10) == "A");
    CHECK_TRUE(PMA_HELPER::byte_to_hex(0x90) == "90");
    CHECK_TRUE(PMA_HELPER::byte_to_hex(0xb) == "B");
    CHECK_TRUE(PMA_HELPER::byte_to_hex(0) == "0");
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

  // test ipv6_addr_to_str
  {
    std::array<uint8_t, 16> ipv6;
    std::memset(ipv6.data(), 0, ipv6.size());

    std::string res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "::");

    ipv6.at(15) = 1;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "::1");

    ipv6.at(14) = 0xf;
    ipv6.at(0) = 0xab;
    ipv6.at(1) = 0xcd;
    ipv6.at(3) = 0xe;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "ABCD:E::F01");

    std::memset(ipv6.data(), 0, ipv6.size());
    ipv6.at(1) = 1;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "1::");

    ipv6.at(2) = 0xf;
    ipv6.at(3) = 0xa;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "1:F0A::");

    std::memset(ipv6.data(), 0, ipv6.size());
    ipv6.at(4) = 0x12;
    ipv6.at(5) = 3;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "::1203:0:0:0:0:0");

    std::memset(ipv6.data(), 0, ipv6.size());
    ipv6.at(6) = 4;
    ipv6.at(7) = 7;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "::407:0:0:0:0");

    std::memset(ipv6.data(), 0, ipv6.size());
    ipv6.at(8) = 0xa;
    ipv6.at(9) = 0xbc;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "::ABC:0:0:0");

    std::memset(ipv6.data(), 0, ipv6.size());
    ipv6.at(10) = 0xed;
    ipv6.at(11) = 0;
    res = PMA_HTTP::ipv6_addr_to_str(ipv6);
    CHECK_TRUE(res == "::ED00:0:0");

    // Some fuzzing with deterministic psuedo-random values
    std::default_random_engine re{0};
    std::uniform_int_distribution int_dist(0, 0xFF);

    std::array<uint8_t, 16> ipv6_result;
    for (int idx = 0; idx < 10000; ++idx) {
      for (int ipv6_idx = 0; ipv6_idx < static_cast<int>(ipv6.size());
           ++ipv6_idx) {
        ipv6.at(ipv6_idx) = static_cast<uint8_t>(int_dist(re));
      }
      if (idx > 5000) {
        int zero_size = (idx - 5000) * 8 / 5000;
        int idx = int_dist(re) % 15;
        if (idx + zero_size >= 15) {
          zero_size = 15 - idx;
        }
        std::memset(ipv6.data() + idx, 0, zero_size);
      }
      res = PMA_HTTP::ipv6_addr_to_str(ipv6);
      ipv6_result = PMA_HTTP::str_to_ipv6_addr(res);
      CHECK_TRUE(ipv6_result == ipv6);
      if (ipv6_result != ipv6) {
        std::println("Started with {}, ended with {}",
                     PMA_HELPER::array_to_str<uint8_t, 16>(ipv6),
                     PMA_HELPER::array_to_str<uint8_t, 16>(ipv6_result));
      }
    }
  }

  // test str_to_ipv4_addr
  {
    uint32_t ipv4_addr = PMA_HTTP::str_to_ipv4_addr("10.123.45.6");
    // To native byte order.
    ipv4_addr = PMA_HELPER::be_swap_u32(ipv4_addr);
    CHECK_TRUE(ipv4_addr == 0x0A7B2D06);

    ipv4_addr = PMA_HTTP::str_to_ipv4_addr("192.168.0.1");
    // To native byte order.
    ipv4_addr = PMA_HELPER::be_swap_u32(ipv4_addr);
    CHECK_TRUE(ipv4_addr == 0xC0A80001);

    try {
      ipv4_addr = PMA_HTTP::str_to_ipv4_addr("256.1.2.3");
      CHECK_TRUE(!"Should have failed to parse \"256.1.2.3\"!");
    } catch (const std::exception &e) {
      CHECK_TRUE("Expected to fail to parse \"256.1.2.3\"!");
    }

    try {
      ipv4_addr = PMA_HTTP::str_to_ipv4_addr("1.2.3.1111");
      CHECK_TRUE(!"Should have failed to parse \"1.2.3.1111\"!");
    } catch (const std::exception &e) {
      CHECK_TRUE("Expected to fail to parse \"1.2.3.1111\"!");
    }
  }

  std::println("{} out of {} tests succeeded", test_succeeded.load(),
               test_count.load());
  return test_succeeded.load() == test_count.load() ? 0 : 1;
}
