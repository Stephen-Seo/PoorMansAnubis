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

#include "helpers.h"

// Standard library includes
#include <cstdio>
#include <cstring>

// Posix includes
#include <signal.h>

// Third party includes
#include <openssl/evp.h>

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

uint16_t PMA_HELPER::le_swap_u16(uint16_t u16) {
  return !is_big_endian() ? u16 : endian_swap_u16(u16);
}

uint32_t PMA_HELPER::le_swap_u32(uint32_t u32) {
  return !is_big_endian() ? u32 : endian_swap_u32(u32);
}

uint64_t PMA_HELPER::le_swap_u64(uint64_t u64) {
  return !is_big_endian() ? u64 : endian_swap_u64(u64);
}

std::string PMA_HELPER::byte_to_hex(uint8_t byte) {
  std::array<char, 3> buf;

  std::snprintf(buf.data(), buf.size(), "%X", byte);

  buf.at(2) = 0;
  return buf.data();
}

std::string PMA_HELPER::ascii_str_to_lower(std::string other) {
  std::string ret;

  for (auto iter = other.begin(); iter != other.end(); ++iter) {
    if (*iter >= 'A' && *iter <= 'Z') {
      ret.push_back((*iter) + 32);
    } else {
      ret.push_back(*iter);
    }
  }

  return ret;
}

void PMA_HELPER::str_replace_all(std::string &body, std::string target,
                                 std::string result) {
  std::string::size_type idx = body.find(target);
  while (idx != std::string::npos) {
    body.replace(idx, target.size(), result);
    idx += result.size();
    idx = body.find(target, idx);
  }
}

int PMA_HELPER::set_signal_handler(int signal, void (*handler)(int)) {
  struct sigaction sa;

  sa.sa_handler = handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  return sigaction(signal, &sa, nullptr);
}

PMA_HELPER::BinaryPart::BinaryPart() : size(0), data(nullptr) {}

PMA_HELPER::BinaryPart::~BinaryPart() {
  if (data && size != 0) {
    delete[] data;
  }
}

PMA_HELPER::BinaryPart::BinaryPart(size_t size, uint8_t *data)
    : size(size), data(data) {}

PMA_HELPER::BinaryPart::BinaryPart(BinaryPart &&other)
    : size(other.size), data(other.data) {
  other.data = nullptr;
}

PMA_HELPER::BinaryPart &PMA_HELPER::BinaryPart::operator=(BinaryPart &&other) {
  if (data && size != 0) {
    delete[] data;
  }

  this->size = other.size;
  this->data = other.data;

  other.data = nullptr;

  return *this;
}

PMA_HELPER::BinaryParts::BinaryParts() : parts() {}

PMA_HELPER::BinaryParts::BinaryParts(BinaryParts &&other)
    : parts(std::move(other.parts)) {
  other.parts = std::list<BinaryPart>();
}

PMA_HELPER::BinaryParts &PMA_HELPER::BinaryParts::operator=(
    BinaryParts &&other) {
  parts = std::move(other.parts);
  other.parts = std::list<BinaryPart>();
  return *this;
}

void PMA_HELPER::BinaryParts::append(size_t size, uint8_t *data) {
  parts.emplace_back(size, data);
}

PMA_HELPER::BinaryPart PMA_HELPER::BinaryParts::combine() const {
  size_t size = 0;
  for (const auto &part : parts) {
    size += part.size;
  }

  if (size == 0) {
    return BinaryPart();
  }

  uint8_t *combined_data = new uint8_t[size];
  size_t idx = 0;
  for (const auto &part : parts) {
    std::memcpy(combined_data + idx, part.data, part.size);
    idx += part.size;
  }

  return BinaryPart(size, combined_data);
}

std::array<uint8_t, 20> PMA_HELPER::sha1_digest(uint8_t *data, size_t size) {
  std::array<uint8_t, 20> ret;

  EVP_MD_CTX *ctx = EVP_MD_CTX_create();
  GenericCleanup<EVP_MD_CTX *> ctx_cleanup(ctx, [](EVP_MD_CTX **ctx) {
    if (ctx && *ctx) {
      EVP_MD_CTX_destroy(*ctx);
      *ctx = nullptr;
    }
  });

  EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
  EVP_DigestUpdate(ctx, data, size);
  EVP_DigestFinal_ex(ctx, ret.data(), nullptr);

  return ret;
}

std::array<char, 40> PMA_HELPER::digest_s20_to_hex(
    const std::array<uint8_t, 20> &digest) {
  std::array<char, 40> ret;

  int temp;
  for (size_t idx = 0; idx < digest.size(); ++idx) {
    temp = digest.at(idx) >> 4;
    if (temp >= 0 && temp <= 9) {
      ret.at(idx * 2) = '0' + static_cast<char>(temp);
    } else if (temp >= 10 && temp <= 15) {
      ret.at(idx * 2) = 'a' + static_cast<char>(temp - 10);
    }

    temp = digest.at(idx) & 0xF;
    if (temp >= 0 && temp <= 9) {
      ret.at(idx * 2 + 1) = '0' + static_cast<char>(temp);
    } else if (temp >= 10 && temp <= 15) {
      ret.at(idx * 2 + 1) = 'a' + static_cast<char>(temp - 10);
    }
  }

  return ret;
}

std::array<char, 40> PMA_HELPER::sha1_digest_hex(uint8_t *data, size_t size) {
  std::array<uint8_t, 20> digest = sha1_digest(data, size);

  return digest_s20_to_hex(digest);
}
