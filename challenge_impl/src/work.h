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

#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_WORK_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_WORK_H_

// Standard library includes.
#include <stdint.h>

// Third party includes.
#include <data_structures/priority_heap.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Work_Factors {
  SDArchiverChunkedArr *value;
  SDArchiverPHeap *factors;
  void *value2;
} Work_Factors;

void work_cleanup_factors(Work_Factors *factors);

Work_Factors work_generate_target_factors(uint64_t digits);

// Returns a string representing the value. If "len_out" is NULL, then the
// returned string is a C-string. If "len_out" is not NULL, then the length
// of the string will be put in "len_out" and the returned string will NOT be
// NULL terminated.
// 
// The returned buffer must be free'd after use.
char *work_factors_value_to_str(Work_Factors work_factors, uint64_t *len_out);

// Same as previous fn, but number string is base64 encoded.
char *work_factors_value_to_str2(Work_Factors work_factors, uint64_t *len_out);

// Returns a string representing the factors. If "len_out" is NULL, then the
// returned string is a C-string. If "len_out" is not NULL, then the length
// of the string will be put in "len_out" and the returned string will NOT be
// NULL terminated.
// 
// The returned buffer must be free'd after use.
char *work_factors_factors_to_str(Work_Factors work_factors, uint64_t *len_out);

// Same as previous fn, but format is "2x5 3x9 5x4..." instead of "2 2 2 2...".
char *work_factors_factors_to_str2(Work_Factors work_factors,
                                   uint64_t *len_out);

// Value is in base64 characters, first char is least significant.
// 1 "quad" is 24 bits of value.
// Must be cleaned up with work_cleanup_factors2().
Work_Factors work_generate_target_factors2(uint64_t quads);
void work_cleanup_factors2(Work_Factors *wf2);

// Returns value as contiguous base64 string where first byte is least significant.
// Must be free'd after use.
char *work_factors2_value_to_str(Work_Factors wf2, uint64_t *len_out);
// Returns factors like work_factors_factors_to_str2.
// Must be free'd after use.
char *work_factors2_factors_to_str(Work_Factors wf2, uint64_t *len_out);

#ifdef __cplusplus
}
#endif

#endif
