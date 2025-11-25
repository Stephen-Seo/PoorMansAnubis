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

#include "db_msql.h"

// Standard library includes.
#include <cstdlib>
#include <cstring>

// Local includes.
#include "helpers.h"

PMA_MSQL::MSQLPacket::MSQLPacket() : packet_length(0), seq(0), body(nullptr) {}

PMA_MSQL::MSQLPacket::~MSQLPacket() {
  if (body) {
    delete[] body;
  }
}

PMA_MSQL::MSQLPacket::MSQLPacket(uint32_t len, uint8_t seq, uint8_t *data)
    : packet_length(len & 0xFFFFFF), seq(seq), body(data) {}

PMA_MSQL::MSQLPacket::MSQLPacket(MSQLPacket &&other) {
  this->packet_length = other.packet_length;
  this->seq = other.seq;
  this->body = other.body;

  other.body = nullptr;
}

PMA_MSQL::MSQLPacket &PMA_MSQL::MSQLPacket::operator=(
    PMA_MSQL::MSQLPacket &&other) {
  this->packet_length = other.packet_length;
  this->seq = other.seq;
  this->body = other.body;

  other.body = nullptr;

  return *this;
}

std::vector<PMA_MSQL::MSQLPacket> PMA_MSQL::create_packets(uint8_t *data,
                                                           size_t data_size,
                                                           uint8_t *seq) {
  std::vector<MSQLPacket> ret;

  size_t offset = 0;
  while (data_size != 0) {
    if (data_size >= 0xFFFFFF) {
      ret.emplace_back(0xFFFFFF, (*seq)++, new uint8_t[0xFFFFFF]);
      std::memcpy(ret.back().body, data + offset, 0xFFFFFF);
      offset += 0xFFFFFF;
      data_size -= 0xFFFFFF;

      if (data_size == 0) {
        ret.emplace_back(0, (*seq)++, nullptr);
      }
    } else {
      ret.emplace_back(static_cast<uint32_t>(data_size), (*seq)++,
                       new uint8_t[data_size]);
      std::memcpy(ret.back().body, data + offset, data_size);
      offset += data_size;
      data_size = 0;
    }
  }

  return ret;
}
