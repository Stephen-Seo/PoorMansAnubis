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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_CXX_BACKEND_DB_MSQL_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_CXX_BACKEND_DB_MSQL_H_

#include <cstdint>
#include <vector>

namespace PMA_MSQL {

struct MSQLPacket {
  MSQLPacket();
  ~MSQLPacket();

  MSQLPacket(uint32_t, uint8_t, uint8_t *);

  // No copy
  MSQLPacket(const MSQLPacket &) = delete;
  MSQLPacket &operator=(const MSQLPacket &) = delete;

  // Allow move
  MSQLPacket(MSQLPacket &&);
  MSQLPacket &operator=(MSQLPacket &&);

  // Treat this as 3 bytes.
  uint32_t packet_length : 24;
  uint8_t seq;
  uint8_t *body;
};

// Copies "data" into the returned packet struct(s).
std::vector<MSQLPacket> create_packets(uint8_t *data, size_t data_size,
                                       uint8_t *seq);

}  // Namespace PMA_MSQL

#endif
