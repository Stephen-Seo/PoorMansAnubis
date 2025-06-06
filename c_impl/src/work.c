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

#include "work.h"

// Standard library includes.
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <stdio.h>
#include <inttypes.h>

// Third-party includes.
#include "data_structures/linked_list.h"
#include "data_structures/priority_heap.h"

// Local includes.
#include "base64.h"

typedef struct SDPMAInternalStringPart {
  uint64_t size;
  char *c_str;
} SDPMAInternalStringPart;

void internal_string_part_free(SDPMAInternalStringPart *part) {
  if (part) {
    if (part->c_str) {
      free(part->c_str);
    }
    free(part);
  }
}

void internal_string_part_free_void(void *data) {
  internal_string_part_free(data);
}

void work_cleanup_factors(Work_Factors *factors) {
  if (factors) {
    if (factors->value) {
      simple_archiver_chunked_array_cleanup(factors->value);
      free(factors->value);
      factors->value = NULL;
    }
    if (factors->factors) {
      simple_archiver_priority_heap_free(&factors->factors);
    }
  }
}

Work_Factors work_generate_target_factors(uint64_t digits) {
  {
    unsigned int seed;
    ssize_t ret = getrandom(&seed, sizeof(unsigned int), 0);
    if (ret != sizeof(unsigned int)) {
      fprintf(stderr, "WARNING: Failed to set random seed!\n");
    }
    srand(seed);
  }

  Work_Factors factors;

  factors.factors = simple_archiver_priority_heap_init();
  SDArchiverChunkedArr temp = simple_archiver_chunked_array_init(NULL, 2);
  SDArchiverChunkedArr temp2 = simple_archiver_chunked_array_init(NULL, 2);
  int_fast8_t first_temp = 1;
  uint16_t *cptr;
  uint16_t c = 1;
  uint16_t c_temp;
  uint16_t carry = 0;
  int r;
  simple_archiver_chunked_array_push(&temp, &c);

  while ((first_temp && simple_archiver_chunked_array_size(&temp) < digits)
         || (!first_temp && simple_archiver_chunked_array_size(&temp2) < digits)) {
    simple_archiver_chunked_array_clear(first_temp ? &temp2 : &temp);
    r = rand();
    if (r < 0) {
      r = -r;
    }
    switch (r % 17) {
      case 0:
        r = 2;
        cptr = malloc(2);
        *cptr = 2;
        simple_archiver_priority_heap_insert(factors.factors, 2, cptr, NULL);
        break;
      case 1:
        r = 3;
        cptr = malloc(2);
        *cptr = 3;
        simple_archiver_priority_heap_insert(factors.factors, 3, cptr, NULL);
        break;
      case 2:
        r = 5;
        cptr = malloc(2);
        *cptr = 5;
        simple_archiver_priority_heap_insert(factors.factors, 5, cptr, NULL);
        break;
      case 3:
        r = 7;
        cptr = malloc(2);
        *cptr = 7;
        simple_archiver_priority_heap_insert(factors.factors, 7, cptr, NULL);
        break;
      case 4:
        r = 11;
        cptr = malloc(2);
        *cptr = 11;
        simple_archiver_priority_heap_insert(factors.factors, 11, cptr, NULL);
        break;
      case 5:
        r = 13;
        cptr = malloc(2);
        *cptr = 13;
        simple_archiver_priority_heap_insert(factors.factors, 13, cptr, NULL);
        break;
      case 6:
        r = 17;
        cptr = malloc(2);
        *cptr = 17;
        simple_archiver_priority_heap_insert(factors.factors, 17, cptr, NULL);
        break;
      case 7:
        r = 19;
        cptr = malloc(2);
        *cptr = 19;
        simple_archiver_priority_heap_insert(factors.factors, 19, cptr, NULL);
        break;
      case 8:
        r = 23;
        cptr = malloc(2);
        *cptr = 23;
        simple_archiver_priority_heap_insert(factors.factors, 23, cptr, NULL);
        break;
      case 9:
        r = 29;
        cptr = malloc(2);
        *cptr = 29;
        simple_archiver_priority_heap_insert(factors.factors, 29, cptr, NULL);
        break;
      case 10:
        r = 31;
        cptr = malloc(2);
        *cptr = 31;
        simple_archiver_priority_heap_insert(factors.factors, 31, cptr, NULL);
        break;
      case 11:
        r = 37;
        cptr = malloc(2);
        *cptr = 37;
        simple_archiver_priority_heap_insert(factors.factors, 37, cptr, NULL);
        break;
      case 12:
        r = 41;
        cptr = malloc(2);
        *cptr = 41;
        simple_archiver_priority_heap_insert(factors.factors, 41, cptr, NULL);
        break;
      case 13:
        r = 43;
        cptr = malloc(2);
        *cptr = 43;
        simple_archiver_priority_heap_insert(factors.factors, 43, cptr, NULL);
        break;
      case 14:
        r = 47;
        cptr = malloc(2);
        *cptr = 47;
        simple_archiver_priority_heap_insert(factors.factors, 47, cptr, NULL);
        break;
      case 15:
        r = 53;
        cptr = malloc(2);
        *cptr = 53;
        simple_archiver_priority_heap_insert(factors.factors, 53, cptr, NULL);
        break;
      case 16:
        r = 59;
        cptr = malloc(2);
        *cptr = 59;
        simple_archiver_priority_heap_insert(factors.factors, 59, cptr, NULL);
        break;
    }
    for (size_t idx = 0;
         idx < simple_archiver_chunked_array_size(first_temp ? &temp : &temp2);
         ++idx) {
      c = *((uint16_t*)simple_archiver_chunked_array_at(first_temp ? &temp : &temp2, idx));
      c *= (uint16_t)r;
      c += carry;
      c_temp = c % 10;
      simple_archiver_chunked_array_push(first_temp ? &temp2 : &temp, &c_temp);
      carry = c / 10;
    }
    while (carry != 0) {
      c_temp = carry % 10;
      simple_archiver_chunked_array_push(first_temp ? &temp2 : &temp, &c_temp);
      carry /= 10;
    }
    first_temp = first_temp ? 0 : 1;
  }

  factors.value = malloc(sizeof(SDArchiverChunkedArr));
  *factors.value = first_temp ? temp : temp2;
  simple_archiver_chunked_array_cleanup(first_temp ? &temp2 : &temp);

  return factors;
}

char *work_factors_value_to_str(Work_Factors work_factors, uint64_t *len_out) {
  const uint64_t size =
    simple_archiver_chunked_array_size(work_factors.value) + 1;
  if (len_out) {
    *len_out = size;
  }
  char *out = malloc(size);
  for (uint64_t idx = size - 1; idx-- > 0;) {
    out[size - idx - 2] =
      (char)(
        *((uint16_t*)simple_archiver_chunked_array_at(work_factors.value, idx))
        + 0x30
      );
  }
  out[size - 1] = 0;
  return out;
}

char *work_factors_value_to_str2(Work_Factors factors, uint64_t *len_out) {
  char *num_str = work_factors_value_to_str(factors, len_out);
  char *b64_str = base64_number_str_to_base64_str(num_str);
  free(num_str);
  if (b64_str) {
    return b64_str;
  } else {
    return NULL;
  }
}

char *work_factors_factors_to_str(Work_Factors work_factors, uint64_t *len_out)
{
  __attribute__((cleanup(simple_archiver_priority_heap_free)))
  SDArchiverPHeap *factors_shallow_clone =
    simple_archiver_priority_heap_clone(work_factors.factors, NULL);
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *string_parts = simple_archiver_list_init();
  uint64_t digits;
  uint16_t temp;
  while (simple_archiver_priority_heap_size(factors_shallow_clone) != 0) {
    const uint16_t *value =
      simple_archiver_priority_heap_pop(factors_shallow_clone);
    temp = *value;
    digits = 0;
    while (temp != 0) {
      temp /= 10;
      ++digits;
    }
    if (digits == 0) {
      digits = 1;
    }
    char *c_str = malloc(digits + 1);
    snprintf(c_str, digits + 1, "%" PRIu16, *value);
    SDPMAInternalStringPart *part = malloc(sizeof(SDPMAInternalStringPart));
    part->size = digits;
    part->c_str = c_str;
    simple_archiver_list_add(string_parts,
                             part,
                             internal_string_part_free_void);
  }

  uint64_t combined_size = 0;
  for (SDArchiverLLNode *node = string_parts->head->next;
       node != string_parts->tail;
       node = node->next) {
    const SDPMAInternalStringPart *part = node->data;
    combined_size += part->size;
    if (node->next != string_parts->tail) {
      combined_size += 1;
    }
  }

  if (len_out) {
    *len_out = combined_size;
    char *combined_buf = malloc(combined_size);
    char *ptr = combined_buf;
    for (SDArchiverLLNode *node = string_parts->head->next;
         node != string_parts->tail;
         node = node->next) {
      const SDPMAInternalStringPart *part = node->data;
      memcpy(ptr, part->c_str, part->size);
      ptr += part->size;
      if (node->next != string_parts->tail) {
        *ptr = ' ';
        ++ptr;
      }
    }
    return combined_buf;
  } else {
    char *combined_buf = malloc(combined_size + 1);
    char *ptr = combined_buf;
    for (SDArchiverLLNode *node = string_parts->head->next;
         node != string_parts->tail;
         node = node->next) {
      const SDPMAInternalStringPart *part = node->data;
      memcpy(ptr, part->c_str, part->size);
      ptr += part->size;
      if (node->next != string_parts->tail) {
        *ptr = ' ';
        ++ptr;
      }
    }
    combined_buf[combined_size] = 0;
    return combined_buf;
  }
}

char *work_factors_factors_to_str2(Work_Factors factors, uint64_t *len_out) {
  __attribute__((cleanup(simple_archiver_priority_heap_free)))
  SDArchiverPHeap *factors_shallow_clone =
    simple_archiver_priority_heap_clone(factors.factors, NULL);
  __attribute__((cleanup(simple_archiver_list_free)))
  SDArchiverLinkedList *string_parts = simple_archiver_list_init();
  uint64_t current = 0;
  uint64_t count = 0;
  while (simple_archiver_priority_heap_size(factors_shallow_clone) != 0) {
    const uint16_t *value =
      simple_archiver_priority_heap_pop(factors_shallow_clone);
    if (current != *value) {
      if (current != 0) {
        size_t digits = 0;
        for (uint64_t temp = current; temp != 0; temp /= 10) {
          ++digits;
        }
        for (uint64_t temp = count; temp != 0; temp /= 10) {
          ++digits;
        }
        // 1 for "x", 1 for NULL terminator.
        digits += 2;
        char *c_str = malloc(digits);
        snprintf(c_str, digits, "%" PRIu64 "x%" PRIu64, current, count);
        c_str[digits - 1] = 0;
        simple_archiver_list_add(string_parts, c_str, NULL);
      }
      current = *value;
      count = 1;
    } else {
      ++count;
    }
  }
  if (current != 0 && count != 0) {
    size_t digits = 0;
    for (uint64_t temp = current; temp != 0; temp /= 10) {
      ++digits;
    }
    for (uint64_t temp = count; temp != 0; temp /= 10) {
      ++digits;
    }
    // 1 for "x", 1 for NULL terminator.
    digits += 2;
    char *c_str = malloc(digits);
    snprintf(c_str, digits, "%" PRIu64 "x%" PRIu64, current, count);
    c_str[digits - 1] = 0;
      simple_archiver_list_add(string_parts, c_str, NULL);
  }

  count = 0;
  current = 1;
  for (SDArchiverLLNode *node = string_parts->head->next;
       node != string_parts->tail;
       node = node->next) {
    count += strlen(node->data);
    if (current) {
      current = 0;
    } else {
      ++count;
    }
  }
  ++count;

  const uint64_t out_count = count;
  char *out = malloc(out_count);
  if (len_out) {
    *len_out = out_count;
  }
  char *temp = out;
  current = 1;
  for (SDArchiverLLNode *node = string_parts->head->next;
       node != string_parts->tail;
       node = node->next) {
    if (current) {
      current = 0;
    } else {
      *temp = ' ';
      temp += 1;
    }
    count = strlen(node->data);
    memcpy(temp, node->data, count);
    temp += count;
  }

  out[out_count - 1] = 0;

  return out;
}
