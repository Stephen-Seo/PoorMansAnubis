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

#include "helpers.h"

// Standard library includes
#include <cstdio>

uint16_t PMA_HELPER::endian_swap_u16(uint16_t u16) {
  uint8_t *u16_ptr = reinterpret_cast<uint8_t *>(&u16);
  uint16_t result;
  uint8_t *res_ptr = reinterpret_cast<uint8_t *>(&result);

  res_ptr[0] = u16_ptr[1];
  res_ptr[1] = u16_ptr[0];

  return result;
}

uint32_t PMA_HELPER::endian_swap_u32(uint32_t u32) {
  uint8_t *u32_ptr = reinterpret_cast<uint8_t *>(&u32);
  uint32_t result;
  uint8_t *res_ptr = reinterpret_cast<uint8_t *>(&result);

  res_ptr[0] = u32_ptr[3];
  res_ptr[1] = u32_ptr[2];
  res_ptr[2] = u32_ptr[1];
  res_ptr[3] = u32_ptr[0];

  return result;
}

uint64_t PMA_HELPER::endian_swap_u64(uint64_t u64) {
  uint8_t *u64_ptr = reinterpret_cast<uint8_t *>(&u64);
  uint64_t result;
  uint8_t *res_ptr = reinterpret_cast<uint8_t *>(&result);

  res_ptr[0] = u64_ptr[7];
  res_ptr[1] = u64_ptr[6];
  res_ptr[2] = u64_ptr[5];
  res_ptr[3] = u64_ptr[4];
  res_ptr[4] = u64_ptr[3];
  res_ptr[5] = u64_ptr[2];
  res_ptr[6] = u64_ptr[1];
  res_ptr[7] = u64_ptr[0];

  return result;
}

uint16_t PMA_HELPER::be_swap_u16(uint16_t u16) {
  return is_big_endian() ? u16 : endian_swap_u16(u16);
}

uint32_t PMA_HELPER::be_swap_u32(uint32_t u32) {
  return is_big_endian() ? u32 : endian_swap_u32(u32);
}

uint64_t PMA_HELPER::be_swap_u64(uint64_t u64) {
  return is_big_endian() ? u64 : endian_swap_u64(u64);
}

std::string PMA_HELPER::byte_to_hex(uint8_t byte) {
  std::array<char, 3> buf;

  std::snprintf(buf.data(), buf.size(), "%X", byte);

  buf.at(2) = 0;
  return buf.data();
}
