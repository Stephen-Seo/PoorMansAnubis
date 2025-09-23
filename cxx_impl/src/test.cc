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

  std::println("{} out of {} tests succeeded", test_succeeded.load(),
               test_count.load());
  return 0;
}
