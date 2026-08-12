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

#include "base64.h"

// Standard library includes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char base64_value_to_base64(unsigned char value) {
  if (value <= 25) {
    return value + 'A';
  } else if (value >= 26 && value <= 51) {
    return (value - 26) + 'a';
  } else if (value >= 52 && value <= 61) {
    return (value - 52) + '0';
  } else if (value == 62) {
    return '+';
  } else if (value == 63) {
    return '/';
  } else {
    return '=';
  }
}

unsigned char base64_base64_to_value(unsigned char b64) {
  if (b64 >= 'A' && b64 <= 'Z') {
    return b64 - 'A';
  } else if (b64 >= 'a' && b64 <= 'z') {
    return b64 - 'a' + 26;
  } else if (b64 >= '0' && b64 <= '9') {
    return b64 - '0' + 52;
  } else if (b64 == '+') {
    return 62;
  } else if (b64 == '/') {
    return 63;
  } else {
    return 0xFF;
  }
}

char *base64_number_str_to_base64_str(const char *n_str) {
  const unsigned long n_str_len = strlen(n_str);
  const size_t enc_size = n_str_len * 2 / 3 + 2;
  char *encoded = malloc(enc_size);
  size_t current = 0;
  size_t current_length = 0;
  size_t temp;
  size_t b64_idx = 0;
  for (const char *iter = n_str; *iter != 0; ++iter) {
    if (*iter < '0' || *iter > '9') {
      fprintf(stderr, "ERROR: Got non-number-str character!\n");
      free(encoded);
      return NULL;
    }
    current = (current << 4) | (size_t)(*iter - '0');
    current_length += 4;
    if (current_length >= 6) {
      temp = current >> (current_length - 6);
      // fprintf(stderr, "DEBUG: Converting 0x%zX\n", temp);
      temp = base64_value_to_base64((unsigned char)temp);
      if (b64_idx >= enc_size) {
        free(encoded);
        return NULL;
      }
      encoded[b64_idx++] = (char)temp;
      current_length -= 6;
      temp = 0;
      for (size_t temp2 = 0; temp2 < current_length; ++temp2) {
        temp = (temp << 1) | 1;
      }
      current &= temp;
    }
  }
  if (current_length == 4) {
    temp = base64_value_to_base64((unsigned char)(current << 2) | 0x3);
    if (b64_idx >= enc_size) {
      free(encoded);
      return NULL;
    }
    encoded[b64_idx++] = (char)temp;
  } else if (current_length == 2) {
    temp = base64_value_to_base64((unsigned char)(current << 4) | 0xF);
    if (b64_idx >= enc_size) {
      free(encoded);
      return NULL;
    }
    encoded[b64_idx++] = (char)temp;
  }

  if (b64_idx >= enc_size) {
    free(encoded);
    return NULL;
  }
  encoded[b64_idx++] = 0;

  return encoded;
}

char *base64_base64_str_to_number_str(const char *b64_str) {
  const unsigned long b64_str_len = strlen(b64_str);
  const size_t dec_size = b64_str_len * 3 / 2 + 1;
  char *dec = malloc(dec_size);
  size_t current = 0;
  size_t current_len = 0;
  size_t temp;
  size_t dec_idx = 0;
  for (const char *iter = b64_str; *iter != 0; ++iter) {
    temp = base64_base64_to_value((unsigned char)*iter);
    if (temp == 0xFF) {
      fprintf(stderr, "ERROR: Invalid conversion of b64 to value!\n");
      free(dec);
      return NULL;
    }
    current = (current << 6) + temp;
    current_len += 6;
    while (current_len >= 4) {
      temp = current_len - 4;
      temp = current >> temp;
      if (temp < 10) {
        if (dec_idx >= dec_size) {
          free(dec);
          return NULL;
        }
        dec[dec_idx++] = (char)temp + '0';
      }
      current_len -= 4;
      temp = 0;
      for (size_t temp2 = 0; temp2 < current_len; ++temp2) {
        temp = (temp << 1) | 1;
      }
      current &= temp;
    }
  }
  if (current_len == 2 && current != 3) {
    fprintf(stderr, "ERROR: Invalid end-state converting b64 to value!\n");
    free(dec);
    return NULL;
  }

  if (dec_idx >= dec_size) {
    free(dec);
    return NULL;
  }
  dec[dec_idx++] = 0;

  return dec;
}

static char INTERNAL_value_to_base64_map[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

char *base64_data_to_base64(const char *data,
                            unsigned long long size,
                            unsigned long long *out_size) {
    if (!data) {
        return NULL;
    }

    const unsigned char *udata = (const unsigned char*)data;

    unsigned long long b64_size = ((size / 3) + (size % 3 != 0 ? 1 : 0)) * 4;
    if (out_size) {
        *out_size = b64_size;
    }

    // +1 for NULL terminator
    char *out = malloc(b64_size + 1);
    if (!out) {
        return NULL;
    }

    unsigned char carry = 0;
    unsigned long long out_idx = 0;
    char prev_state = 2;
    for (unsigned long long idx = 0; idx < size; ++ idx) {
        if (out_idx >= b64_size) {
            free(out);
            return NULL;
        }

        switch(idx % 3) {
          case 0: {
            unsigned char b64_part = udata[idx] >> 2;
            out[out_idx++] = INTERNAL_value_to_base64_map[b64_part];
            carry = udata[idx] & 0x3;

            prev_state = 0;
          } break;

          case 1: {
            unsigned char b64_part =
                (carry << 4)
                | (udata[idx] >> 4);
            out[out_idx++] = INTERNAL_value_to_base64_map[b64_part];
            carry = udata[idx] & 0xF;

            prev_state = 1;
          } break;

          case 2: {
            unsigned char b64_part =
                (carry << 2)
                | (udata[idx] >> 6);
            out[out_idx++] = INTERNAL_value_to_base64_map[b64_part];

            if (out_idx >= b64_size) {
                free(out);
                return NULL;
            }
            b64_part = udata[idx] & 0x3F;
            out[out_idx++] = INTERNAL_value_to_base64_map[b64_part];

            prev_state = 2;
          } break;
        }
    }

    switch (prev_state) {
      case 0: {
        unsigned char b64_part = carry << 4;
        out[out_idx++] = INTERNAL_value_to_base64_map[b64_part];
        out[out_idx++] = '=';
        out[out_idx++] = '=';
      } break;

      case 1: {
        unsigned char b64_part = carry << 2;
        out[out_idx++] = INTERNAL_value_to_base64_map[b64_part];
        out[out_idx++] = '=';
      } break;

      case 2:
        break;
    }

    if (out_idx != b64_size) {
        free(out);
        return NULL;
    }

    out[b64_size] = 0;
    return out;
}

static unsigned char INTERNAL_base64_to_value_map[256] = {
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    62, // '+'
    255,
    62, // '-'
    255,
    63, // '/'
    52, // '0'
    53, // '1'
    54, // '2'
    55, // '3'
    56, // '4'
    57, // '5'
    58, // '6'
    59, // '7'
    60, // '8'
    61, // '9'
255, 255, 255, 255, 255, 255, 255,
    0, // 'A'
    1, // 'B'
    2, // 'C'
    3, // 'D'
    4, // 'E'
    5, // 'F'
    6, // 'G'
    7, // 'H'
    8, // 'I'
    9, // 'J'
    10, // 'K'
    11, // 'L'
    12, // 'M'
    13, // 'N'
    14, // 'O'
    15, // 'P'
    16, // 'Q'
    17, // 'R'
    18, // 'S'
    19, // 'T'
    20, // 'U'
    21, // 'V'
    22, // 'W'
    23, // 'X'
    24, // 'Y'
    25, // 'Z'
255, 255, 255, 255, 255, 255,
    26, // 'a'
    27, // 'b'
    28, // 'c'
    29, // 'd'
    30, // 'e'
    31, // 'f'
    32, // 'g'
    33, // 'h'
    34, // 'i'
    35, // 'j'
    36, // 'k'
    37, // 'l'
    38, // 'm'
    39, // 'n'
    40, // 'o'
    41, // 'p'
    42, // 'q'
    43, // 'r'
    44, // 's'
    45, // 't'
    46, // 'u'
    47, // 'v'
    48, // 'w'
    49, // 'x'
    50, // 'y'
    51, // 'z'
255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
};

unsigned char *base64_base64_to_data(const char *b64,
                                     unsigned long long size,
                                     unsigned long long *out_size) {
    if (!b64) {
        return NULL;
    }

    unsigned long long d_size = size / 4 * 3;

    if (d_size == 0) {
        return NULL;
    }

    for (size_t idx = size; idx-- > 0;) {
        if (b64[idx] == '=') {
            --d_size;
        } else {
            break;
        }
    }

    if (d_size == 0) {
        return NULL;
    }

    if (out_size) {
        *out_size = d_size;
    }

    unsigned char *data = malloc(d_size);

    size_t d_idx = 0;
    // Set to 10 if '=' encountered.
    char prev_state = 3;
    unsigned char carry = 0;
    for (unsigned long long idx = 0; idx < size; ++idx) {
        switch (idx % 4) {
          case 0: {
            carry = INTERNAL_base64_to_value_map[(unsigned char)b64[idx]];
            if (carry >= 64) {
                free(data);
                return NULL;
            }
          } break;

          case 1: {
            unsigned char val = INTERNAL_base64_to_value_map[(unsigned char)b64[idx]];
            if (val >= 64) {
                free(data);
                return NULL;
            }
            data[d_idx++] = (carry << 2) | ((val & 0x30) >> 4);
            carry = val & 0xF;
          } break;

          case 2: {
            if (b64[idx] == '=') {
                prev_state = 10;
                break;
            }
            unsigned char val = INTERNAL_base64_to_value_map[(unsigned char)b64[idx]];
            if (val >= 64) {
                free(data);
                return NULL;
            }
            data[d_idx++] = (carry << 4) | ((val & 0x3C) >> 2);
            carry = val & 0x3;
          } break;

          case 3: {
            if (b64[idx] == '=') {
                prev_state = 10;
                break;
            }
            unsigned char val = INTERNAL_base64_to_value_map[(unsigned char)b64[idx]];
            if (val >= 64) {
                free(data);
                return NULL;
            }
            data[d_idx++] = (carry << 6) | val;
            carry = 0;
          } break;
        } // switch (idx % 4)

        if (prev_state == 10) {
            break;
        }
    }

    if (d_idx != d_size) {
        free(data);
        return NULL;
    }

    return data;
}
