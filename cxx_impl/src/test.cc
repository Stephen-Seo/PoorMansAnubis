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
#include <random>

// local includes
#include "args.h"
#include "helpers.h"
#include "http.h"
#include "poor_mans_print.h"

#define ASSERT_TRUE(x)                                                 \
  test_count.fetch_add(1);                                             \
  if (!(x)) {                                                          \
    PMA_EPrintln("ERROR: \"{}\" failed assert at {}!", #x, __LINE__);  \
    PMA_Println("{} out of {} tests succeeded", test_succeeded.load(), \
                test_count.load());                                    \
    return 1;                                                          \
  } else {                                                             \
    test_succeeded.fetch_add(1);                                       \
  }

#define CHECK_TRUE(x)                                                 \
  test_count.fetch_add(1);                                            \
  if (!(x)) {                                                         \
    PMA_EPrintln("ERROR: \"{}\" failed assert at {}!", #x, __LINE__); \
  } else {                                                            \
    test_succeeded.fetch_add(1);                                      \
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

    // With square brackets
    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[::1]");
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
    } catch (const std::exception &e) {
      CHECK_TRUE(!"Parsing [::1] should have been valid!");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[1234::5678]");
      CHECK_TRUE(ipv6.at(0) == 0x12);
      CHECK_TRUE(ipv6.at(1) == 0x34);
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
      CHECK_TRUE(ipv6.at(14) == 0x56);
      CHECK_TRUE(ipv6.at(15) == 0x78);
    } catch (const std::exception &e) {
      CHECK_TRUE(!"Parsing [1234::5678] should have been valid!");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[::1");
      CHECK_TRUE(!"Should have failed to parse \"[::1\"");
    } catch (const std::exception &e) {
      // Intentionally left blank.
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("::1]");
      CHECK_TRUE(!"Should have failed to parse \"::1]\"");
    } catch (const std::exception &e) {
      // Intentionally left blank.
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr(
          "[1234:5678:abcd:ef90:1234:5678:abcd:ef90]");
      CHECK_TRUE(ipv6.at(0) == 0x12);
      CHECK_TRUE(ipv6.at(1) == 0x34);
      CHECK_TRUE(ipv6.at(2) == 0x56);
      CHECK_TRUE(ipv6.at(3) == 0x78);
      CHECK_TRUE(ipv6.at(4) == 0xab);
      CHECK_TRUE(ipv6.at(5) == 0xcd);
      CHECK_TRUE(ipv6.at(6) == 0xef);
      CHECK_TRUE(ipv6.at(7) == 0x90);
      CHECK_TRUE(ipv6.at(8) == 0x12);
      CHECK_TRUE(ipv6.at(9) == 0x34);
      CHECK_TRUE(ipv6.at(10) == 0x56);
      CHECK_TRUE(ipv6.at(11) == 0x78);
      CHECK_TRUE(ipv6.at(12) == 0xab);
      CHECK_TRUE(ipv6.at(13) == 0xcd);
      CHECK_TRUE(ipv6.at(14) == 0xef);
      CHECK_TRUE(ipv6.at(15) == 0x90);
    } catch (const std::exception &e) {
      CHECK_TRUE(!"Parsing [1234:5678:abcd:ef90:1234:5678:abcd:ef90] should have been valid!");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[1234::1234:5678:abcd:ef90]");
      CHECK_TRUE(ipv6.at(0) == 0x12);
      CHECK_TRUE(ipv6.at(1) == 0x34);
      CHECK_TRUE(ipv6.at(2) == 0);
      CHECK_TRUE(ipv6.at(3) == 0);
      CHECK_TRUE(ipv6.at(4) == 0);
      CHECK_TRUE(ipv6.at(5) == 0);
      CHECK_TRUE(ipv6.at(6) == 0);
      CHECK_TRUE(ipv6.at(7) == 0);
      CHECK_TRUE(ipv6.at(8) == 0x12);
      CHECK_TRUE(ipv6.at(9) == 0x34);
      CHECK_TRUE(ipv6.at(10) == 0x56);
      CHECK_TRUE(ipv6.at(11) == 0x78);
      CHECK_TRUE(ipv6.at(12) == 0xab);
      CHECK_TRUE(ipv6.at(13) == 0xcd);
      CHECK_TRUE(ipv6.at(14) == 0xef);
      CHECK_TRUE(ipv6.at(15) == 0x90);
    } catch (const std::exception &e) {
      CHECK_TRUE(
          !"Parsing [1234::1234:5678:abcd:ef90] should have been valid!");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[1234:5678::ef90]");
      CHECK_TRUE(ipv6.at(0) == 0x12);
      CHECK_TRUE(ipv6.at(1) == 0x34);
      CHECK_TRUE(ipv6.at(2) == 0x56);
      CHECK_TRUE(ipv6.at(3) == 0x78);
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
      CHECK_TRUE(ipv6.at(14) == 0xef);
      CHECK_TRUE(ipv6.at(15) == 0x90);
    } catch (const std::exception &e) {
      CHECK_TRUE(!"Parsing [1234:5678::ef90] should have been valid!");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[::1:22:333]");
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
      CHECK_TRUE(ipv6.at(11) == 1);
      CHECK_TRUE(ipv6.at(12) == 0);
      CHECK_TRUE(ipv6.at(13) == 0x22);
      CHECK_TRUE(ipv6.at(14) == 0x3);
      CHECK_TRUE(ipv6.at(15) == 0x33);
    } catch (const std::exception &e) {
      CHECK_TRUE(!"Parsing [::1:22:333] should have been valid!");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[111:22:3::]");
      CHECK_TRUE(ipv6.at(0) == 1);
      CHECK_TRUE(ipv6.at(1) == 0x11);
      CHECK_TRUE(ipv6.at(2) == 0);
      CHECK_TRUE(ipv6.at(3) == 0x22);
      CHECK_TRUE(ipv6.at(4) == 0);
      CHECK_TRUE(ipv6.at(5) == 3);
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
    } catch (const std::exception &e) {
      CHECK_TRUE(!"Parsing [111:22:3::] should have been valid!");
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[1234::abcd]]");
      CHECK_TRUE(!"Should have failed to parse [1234::abcd]]");
    } catch (const std::exception &e) {
      // Intentionally left blank.
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[[1234::abcd]");
      CHECK_TRUE(!"Should have failed to parse [[1234::abcd]");
    } catch (const std::exception &e) {
      // Intentionally left blank.
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[1234:abcd]");
      CHECK_TRUE(!"Should have failed to parse [1234:abcd]");
    } catch (const std::exception &e) {
      // Intentionally left blank.
    }

    try {
      ipv6 = PMA_HTTP::str_to_ipv6_addr("[1234.abcd]");
      CHECK_TRUE(!"Should have failed to parse [1234.abcd]");
    } catch (const std::exception &e) {
      // Intentionally left blank.
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
        PMA_Println("Started with {}, ended with {}",
                    PMA_HELPER::array_to_str<uint8_t, 16>(ipv6),
                    PMA_HELPER::array_to_str<uint8_t, 16>(ipv6_result));
      }
    }
  }

  // test str_to_ipv4_addr
  {
    uint32_t ipv4_addr =
        PMA_HELPER::be_swap_u32(PMA_HTTP::str_to_ipv4_addr("10.123.45.6"));
    CHECK_TRUE(ipv4_addr == 0x0A7B2D06);

    ipv4_addr =
        PMA_HELPER::be_swap_u32(PMA_HTTP::str_to_ipv4_addr("192.168.0.1"));
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

  // test ipv4_addr_to_str
  {
    union {
      uint32_t u32;
      std::array<uint8_t, 4> u8_arr;
    } addr_u, addr_u_res;

    uint32_t ipv4_addr = PMA_HELPER::be_swap_u32(0x7F000001);
    std::string ret = PMA_HTTP::ipv4_addr_to_str(ipv4_addr);
    CHECK_TRUE(ret == "127.0.0.1");

    // Some fuzzing with deterministic psuedo-random values
    std::default_random_engine re{0};
    std::uniform_int_distribution int_dist(0, 0xFF);

    for (int idx = 0; idx < 10000; ++idx) {
      for (int addr_idx = 0; addr_idx < 4; ++addr_idx) {
        addr_u.u8_arr.at(addr_idx) = static_cast<uint8_t>(int_dist(re));
      }
      ret = PMA_HTTP::ipv4_addr_to_str(PMA_HELPER::be_swap_u32(addr_u.u32));
      addr_u_res.u32 = PMA_HELPER::be_swap_u32(PMA_HTTP::str_to_ipv4_addr(ret));
      CHECK_TRUE(addr_u.u32 == addr_u_res.u32);
      if (addr_u.u32 != addr_u_res.u32) {
        PMA_Println("Started with {}, ended with {}",
                    PMA_HELPER::array_to_str<uint8_t, 4>(addr_u.u8_arr),
                    PMA_HELPER::array_to_str<uint8_t, 4>(addr_u_res.u8_arr));
      }
    }
  }

  // test Arg parsing
  {
    const char *argv[] = {"program",
                          "--factors=10",
                          "--dest-url=http://127.0.0.1:9000/",
                          "--addr-port=127.0.0.1:8088",
                          "--port-to-dest-url=8088:http://127.0.0.1:9001/",
                          "--enable-x-real-ip-header",
                          "--api-url=/pma_api_url",
                          "--js-factors-url=/pma_factors_url.js",
                          "--challenge-timeout=2",
                          "--allowed-timeout=30",
                          nullptr};
    PMA_ARGS::Args args(10, const_cast<char **>(argv));
    ASSERT_TRUE(!args.flags.test(2));
    CHECK_TRUE(args.factors == 10);
    CHECK_TRUE(args.default_dest_url == "http://127.0.0.1:9000/");
    {
      PMA_ARGS::AddrPort addr_port = {"127.0.0.1", 8088};
      CHECK_TRUE(args.addr_ports.size() == 1);
      CHECK_TRUE(std::get<0>(args.addr_ports.at(0)) == std::get<0>(addr_port));
      CHECK_TRUE(std::get<1>(args.addr_ports.at(0)) == std::get<1>(addr_port));
    }
    {
      auto iter = args.port_to_dest_urls.find(8088);
      CHECK_TRUE(iter != args.port_to_dest_urls.end());
      if (iter != args.port_to_dest_urls.end()) {
        CHECK_TRUE(iter->second == "http://127.0.0.1:9001/");
      }
    }
    CHECK_TRUE(args.flags.test(0));
    CHECK_TRUE(!args.flags.test(1));
    CHECK_TRUE(!args.flags.test(3));
    CHECK_TRUE(args.api_url == "/pma_api_url/");
    CHECK_TRUE(args.js_factors_url == "/pma_factors_url.js");
    CHECK_TRUE(args.challenge_timeout == 2);
    CHECK_TRUE(args.allowed_timeout == 30);
  }

  // Test PMA_HELPER::ascii_str_to_lower(...)
  {
    CHECK_TRUE("apple_banana_zebra" ==
               PMA_HELPER::ascii_str_to_lower("APPLE_BANANA_ZEBRA"));
  }

  // Test PMA_HTTP::parse_simple_json(...)
  {
    const auto [err, unordmap] = PMA_HTTP::parse_simple_json(
        "{ \"key\":\"value\", \"one\": \"1\", \"left\" : \"right\" }");
    auto iter = unordmap.find("key");
    CHECK_TRUE(iter != unordmap.end());
    if (iter != unordmap.end()) {
      CHECK_TRUE(iter->second == "value");
    }

    iter = unordmap.find("one");
    CHECK_TRUE(iter != unordmap.end());
    if (iter != unordmap.end()) {
      CHECK_TRUE(iter->second == "1");
    }

    iter = unordmap.find("left");
    CHECK_TRUE(iter != unordmap.end());
    if (iter != unordmap.end()) {
      CHECK_TRUE(iter->second == "right");
    }
  }

  PMA_Println("{} out of {} tests succeeded", test_succeeded.load(),
              test_count.load());
  return test_succeeded.load() == test_count.load() ? 0 : 1;
}
