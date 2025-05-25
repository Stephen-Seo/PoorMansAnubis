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

#include "base64.h"

// Standard library includes.
#include <stdio.h>
#include <stdlib.h>

// Third party library includes.
#include <data_structures/linked_list.h>
#include <helpers.h>

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
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *list = simple_archiver_list_init();
  size_t current = 0;
  size_t current_length = 0;
  size_t temp;
  size_t b64_length = 1;
  for (const char *iter = n_str; *iter != 0; ++iter) {
    current = (current << 4) | (size_t)(*iter - '0');
    current_length += 4;
    if (current_length >= 6) {
      temp = current >> (current_length - 6);
      // fprintf(stderr, "DEBUG: Converting 0x%zX\n", temp);
      temp = base64_value_to_base64((unsigned char)temp);
      simple_archiver_list_add(list,
                               (void*)temp,
                               simple_archiver_helper_datastructure_cleanup_nop);
      ++b64_length;
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
    simple_archiver_list_add(list,
                             (void*)temp,
                             simple_archiver_helper_datastructure_cleanup_nop);
    ++b64_length;
  } else if (current_length == 2) {
    temp = base64_value_to_base64((unsigned char)(current << 4) | 0xF);
    simple_archiver_list_add(list,
                             (void*)temp,
                             simple_archiver_helper_datastructure_cleanup_nop);
    ++b64_length;
  }

  char *encoded = malloc(b64_length);
  temp = 0;
  for (SDArchiverLLNode *node = list->head->next;
       node != list->tail;
       node = node->next) {
    encoded[temp++] = (char)((size_t)node->data);
  }
  encoded[b64_length - 1] = 0;

  return encoded;
}

char *base64_base64_str_to_number_str(const char *b64_str) {
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *list = simple_archiver_list_init();
  size_t current = 0;
  size_t current_len = 0;
  size_t temp;
  size_t dec_length = 1;
  for (const char *iter = b64_str; *iter != 0; ++iter) {
    temp = base64_base64_to_value((unsigned char)*iter);
    if (temp == 0xFF) {
      fprintf(stderr, "ERROR: Invalid conversion of b64 to value!\n");
      return NULL;
    }
    current = (current << 6) + temp;
    current_len += 6;
    while (current_len >= 4) {
      temp = current_len - 4;
      temp = current >> temp;
      if (temp < 10) {
        simple_archiver_list_add(
          list, (void*)temp, simple_archiver_helper_datastructure_cleanup_nop);
        ++dec_length;
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
    return NULL;
  }

  char *dec = malloc(dec_length);
  temp = 0;
  for (SDArchiverLLNode *node = list->head->next;
       node != list->tail;
       node = node->next) {
    dec[temp++] = (char)((size_t)node->data) + '0';
  }
  dec[dec_length - 1] = 0;

  return dec;
}
