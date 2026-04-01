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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>

// Posix includes
#include <signal.h>

// Third party includes
#include <openssl/evp.h>

std::string PMA_HELPER::vec_to_hexadecimal(const std::vector<uint8_t> &data) {
  std::string hex;

  for (uint8_t value : data) {
    uint8_t upper = (value >> 4) & 0xF;
    uint8_t lower = value & 0xF;

    if (upper < 0xA) {
      hex.push_back(static_cast<char>('0' + upper));
    } else {
      hex.push_back(static_cast<char>('A' + upper - 10));
    }

    if (lower < 0xA) {
      hex.push_back(static_cast<char>('0' + lower));
    } else {
      hex.push_back(static_cast<char>('A' + lower - 10));
    }
  }

  return hex;
}

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

std::string PMA_HELPER::trim_whitespace(const std::string &s) {
  if (s.empty()) {
    return {};
  }

  std::optional<size_t> leading_idx;
  for (size_t idx = 0; idx < s.size(); ++idx) {
    if (s.at(idx) == ' ' || s.at(idx) == '\t' || s.at(idx) == '\r' ||
        s.at(idx) == '\n') {
      leading_idx = idx;
    } else {
      break;
    }
  }

  std::size_t ending_idx = s.size();
  for (size_t idx = ending_idx; idx-- > 0;) {
    if (s.at(idx) == ' ' || s.at(idx) == '\t' || s.at(idx) == '\r' ||
        s.at(idx) == '\n') {
      ending_idx = idx;
    } else {
      break;
    }
  }

  if (leading_idx.has_value()) {
    if (leading_idx.value() + 1 >= ending_idx) {
      return {};
    }

    return s.substr(leading_idx.value() + 1,
                    ending_idx - leading_idx.value() - 1);
  } else {
    return s.substr(0, ending_idx);
  }
}

std::string PMA_HELPER::get_file_ext(const std::string &s) {
  std::string ret;
  for (size_t idx = s.size(); idx-- > 0;) {
    if ((s.at(idx) >= 'a' && s.at(idx) <= 'z') ||
        (s.at(idx) >= 'A' && s.at(idx) <= 'Z') ||
        (s.at(idx) >= '0' && s.at(idx) <= '9')) {
      ret.push_back(s.at(idx));
    } else if (s.at(idx) == '.') {
      break;
    } else {
      return {};
    }
  }

  // Reverse the string since it was built in reverse order.
  std::string real_ret;
  for (size_t idx = ret.size(); idx-- > 0;) {
    real_ret.push_back(ret.at(idx));
  }
  return real_ret;
}

PMA_HELPER::MimeTypes::MimeTypes() : flags(), ext_to_mime_type() {
  std::ifstream ifs("/etc/nginx/mime.types");
  if (!ifs.good()) {
    ifs = std::ifstream("/etc/mime.types");
    if (!ifs.good()) {
      return;
    }
  }

  std::array<char, 256> buf;
  std::string mime_type;
  std::string temp;
  while (!ifs.eof()) {
    mime_type.clear();
    temp.clear();

    ifs.getline(buf.data(), buf.size());
    const auto gcount = ifs.gcount() - 1;

    if (gcount > 0) {
      const size_t count = static_cast<const size_t>(gcount);

      int_fast8_t force_continue = 0;
      for (size_t idx = 0; idx < count; ++idx) {
        char next = buf.at(idx);
        if (next == '#' || next == '{' || next == '}' || next == 0) {
          force_continue = 1;
          break;
        } else if (mime_type.empty()) {
          if (next == ' ' || next == '\t') {
            if (temp.find('/') != std::string::npos) {
              mime_type = PMA_HELPER::trim_whitespace(temp);
            }
            temp.clear();
          } else {
            temp.push_back(next);
          }
        } else {
          if (next == ' ' || next == '\t' || next == ';') {
            if (!temp.empty()) {
              ext_to_mime_type.insert(
                  std::make_pair(PMA_HELPER::trim_whitespace(temp), mime_type));
              temp.clear();
            }
          } else {
            temp.push_back(next);
          }
        }
      }

      if (force_continue) {
        continue;
      } else if (!temp.empty() && !mime_type.empty()) {
        ext_to_mime_type.insert(
            std::make_pair(PMA_HELPER::trim_whitespace(temp), mime_type));
      }
    }
  }

  if (!ext_to_mime_type.empty()) {
    flags.set(0);
  }
}

bool PMA_HELPER::MimeTypes::is_loaded() const { return flags.test(0); }

std::string PMA_HELPER::MimeTypes::get_mimetype_from_ext(
    const std::string &ext) const {
  if (auto iter = ext_to_mime_type.find(ext); iter != ext_to_mime_type.end()) {
    return iter->second;
  }
  return {};
}

uint64_t PMA_HELPER::rand_uint64_t() {
  std::random_device rd{};
  static std::default_random_engine re(rd());
  static std::uniform_int_distribution<uint64_t> int_dist;
  return int_dist(re);
}

uint64_t PMA_HELPER::rng_next_id(uint64_t value) {
  constexpr uint64_t a = 9;
  constexpr uint64_t c = 31;

  std::default_random_engine default_re(value * a + c);

  return std::uniform_int_distribution<uint64_t>()(default_re);
}

std::string PMA_HELPER::next_hash(
    uint64_t value,
    std::vector<uint8_t> (*hasher_fn)(void *data, size_t size)) {
  uint64_t next_id = rng_next_id(value);
  uint64_t random_val = rand_uint64_t();

  std::array<uint8_t, 16> data;
  std::memcpy(data.data(), &next_id, 8);
  std::memcpy(data.data() + 8, &random_val, 8);

  std::vector<uint8_t> hash(hasher_fn(data.data(), data.size()));

  return PMA_HELPER::vec_to_hexadecimal(hash);
}
