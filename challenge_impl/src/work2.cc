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

extern "C" {
#include <data_structures/priority_heap.h>
}

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "base64.h"
#include "work.h"

#ifndef NDEBUG
#include <iostream>
#endif

void internal_chunked_arr_value_cleanup_nop(void *) {}

std::vector<char> sum_b64(const std::vector<char> &a,
                          const std::vector<char> &b) {
  std::vector<char> ret;
  unsigned char carry = 0;

  size_t idx = 0;
  for (; idx < a.size() && idx < b.size(); ++idx) {
    unsigned char av = base64_base64_to_value(a[idx]);
    unsigned char bv = base64_base64_to_value(b[idx]);
    unsigned char sum = static_cast<unsigned char>(av + bv) + carry;
    if (sum >= 64) {
      carry = 1;
      sum -= 64;
    } else {
      carry = 0;
    }
    ret.push_back(base64_value_to_base64(sum));
  }
  for (; idx < a.size(); ++idx) {
    unsigned char av = base64_base64_to_value(a[idx]) + carry;
    if (av >= 64) {
      carry = 1;
      av -= 64;
    } else {
      carry = 0;
    }
    ret.push_back(base64_value_to_base64(av));
  }
  for (; idx < b.size(); ++idx) {
    unsigned char bv = base64_base64_to_value(b[idx]) + carry;
    if (bv >= 64) {
      carry = 1;
      bv -= 64;
    } else {
      carry = 0;
    }
    ret.push_back(base64_value_to_base64(bv));
  }
  if (carry != 0) {
    ret.push_back('B');
  }

  return ret;
}

void sum_b64_scalar(std::vector<char> &a, char b64char) {
  unsigned char carry = base64_base64_to_value(b64char);
  for (size_t idx = 0; idx < a.size(); ++idx) {
    unsigned char sum = base64_base64_to_value(a[idx]) + carry;
    if (sum >= 64) {
      carry = 1;
      sum -= 64;
    } else {
      carry = 0;
    }
    a[idx] = base64_value_to_base64(sum);
  }
  if (carry != 0) {
    a.push_back('B');
  }
}

std::vector<char> mult_b64(const std::vector<char> &a,
                           const std::vector<char> &b) {
  std::vector<char> ret;

  for (size_t aidx = 0; aidx < a.size(); ++aidx) {
    std::vector<char> temp;
    for (size_t aidx2 = 0; aidx2 < aidx; ++aidx2) {
      temp.push_back('A');
    }
    const uint64_t av = base64_base64_to_value(a[aidx]);
    uint64_t carry = 0;
    for (size_t bidx = 0; bidx < b.size(); ++bidx) {
      uint64_t prod =
          av * static_cast<uint64_t>(base64_base64_to_value(b[bidx])) + carry;
      carry = 0;
      if (prod >= 64) {
        carry = prod / 64;
        prod = prod % 64;
      }
      temp.push_back(base64_value_to_base64(static_cast<unsigned char>(prod)));
    }
    while (carry != 0) {
      if (carry >= 64) {
        temp.push_back(base64_value_to_base64(carry % 64));
        carry /= 64;
      } else {
        temp.push_back(base64_value_to_base64(static_cast<unsigned char>(carry)));
        carry = 0;
      }
    }
    ret = sum_b64(ret, temp);
  }

  return ret;
}

void mult_b64_scalar(std::vector<char> &a, unsigned int scalar) {
  unsigned int carry = 0;
  for (size_t idx = 0; idx < a.size(); ++idx) {
    unsigned int prod = base64_base64_to_value(a[idx]) * scalar + carry;
    if (prod >= 64) {
      carry = prod / 64;
      prod = prod % 64;
    } else {
      carry = 0;
    }
    a[idx] = base64_value_to_base64(static_cast<unsigned char>(prod));
  }
  if (carry != 0) {
    a.push_back(base64_value_to_base64(static_cast<unsigned char>(carry)));
  }
}

Work_Factors work_generate_target_factors2(uint64_t quads) {
  {
    srand(std::random_device()());
  }

  Work_Factors wf;
  wf.value = nullptr;
  wf.value2 = std::malloc(sizeof(std::vector<char>));
  std::vector<char> *b64 = new (wf.value2) std::vector<char>();
  b64->push_back('B');
  wf.factors = simple_archiver_priority_heap_init();

  while (b64->size() / 4 < quads) {
    int r = rand();
    if (r < 0) {
      r = -r;
    }
    switch (r % 17) {
      case 0:
        r = 2;
        break;
      case 1:
        r = 3;
        break;
      case 2:
        r = 5;
        break;
      case 3:
        r = 7;
        break;
      case 4:
        r = 11;
        break;
      case 5:
        r = 13;
        break;
      case 6:
        r = 17;
        break;
      case 7:
        r = 19;
        break;
      case 8:
        r = 23;
        break;
      case 9:
        r = 29;
        break;
      case 10:
        r = 31;
        break;
      case 11:
        r = 37;
        break;
      case 12:
        r = 41;
        break;
      case 13:
        r = 43;
        break;
      case 14:
        r = 47;
        break;
      case 15:
        r = 53;
        break;
      case 16:
        r = 59;
        break;
    }
    uint16_t *ptr = static_cast<uint16_t *>(std::malloc(2));
    *ptr = static_cast<uint16_t>(r);
    simple_archiver_priority_heap_insert(wf.factors, r, ptr, nullptr);
    mult_b64_scalar(*b64, r);
  }

  while (b64->size() % 4 != 0) {
    uint16_t *ptr = static_cast<uint16_t *>(std::malloc(2));
    *ptr = 2;
    simple_archiver_priority_heap_insert(wf.factors, 2, ptr, nullptr);
    mult_b64_scalar(*b64, 2);
  }

  return wf;
}

void work_cleanup_factors2(Work_Factors *wf2) {
  if (wf2) {
    if (wf2->value2) {
      reinterpret_cast<std::vector<char> *>(wf2->value2)->~vector();
      std::free(wf2->value2);
      wf2->value2 = nullptr;
    }
    if (wf2->factors) {
      simple_archiver_priority_heap_free(&wf2->factors);
    }
  }
}

char *work_factors2_value_to_str(Work_Factors wf2, uint64_t *len_out) {
  const std::vector<char> *b64 =
      reinterpret_cast<std::vector<char> *>(wf2.value2);
  if (len_out) {
    *len_out = b64->size();
  }

  char *ret = reinterpret_cast<char *>(std::malloc(b64->size() + 1));
  std::memcpy(ret, b64->data(), b64->size());
  ret[b64->size()] = 0;
  return ret;
}

char *work_factors2_factors_to_str(Work_Factors wf2, uint64_t *len_out) {
  return work_factors_factors_to_str2(wf2, len_out);
}
