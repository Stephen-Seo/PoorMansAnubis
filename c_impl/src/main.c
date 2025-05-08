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

// Standard library includes.
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// Local includes.
#include "work.h"

void print_help(void) {
  puts("Usage:");
  puts("  --factors=<digits> : Generate work for factors of a large number "
       "with <digits> digits.");
}

void free_work_factors(Work_Factors **factors) {
  if (*factors) {
    work_cleanup_factors(*factors);
    free(*factors);
  }
}

int main(int argc, char **argv) {
  __attribute__((cleanup(free_work_factors)))
  Work_Factors *work_factors = NULL;

  --argc;
  ++argv;
  while (argc > 0) {
    if (strncmp("--factors=", argv[0], 10) == 0) {
      unsigned long digits = strtoul(argv[0] + 10, NULL, 10);
      if (digits == 0) {
        fprintf(stderr, "ERROR: Could not convert arg \"%s\" digits!", argv[0]);
        return 2;
      } else if (digits == ULONG_MAX) {
        fprintf(stderr, "ERROR: \"%s\" is out of range!", argv[0]);
        return 3;
      } else {
        work_factors = malloc(sizeof(Work_Factors));
        *work_factors = work_generate_target_factors((uint64_t)digits);
      }
    } else {
      fprintf(stderr, "ERROR: Invalid arg \"%s\"!", argv[0]);
      print_help();
      return 1;
    }

    --argc;
    ++argv;
  }

  if (work_factors) {
    {
      char *c_str = work_factors_value_to_str(*work_factors, NULL);
      printf("%s", c_str);
      free(c_str);

      // uint64_t out_size;
      // char *buf = work_factors_value_to_str(*work_factors, &out_size);
      // printf("%.*s", out_size, buf);
      // free(buf);
    }
    printf("\n");

    {
      char *c_str = work_factors_factors_to_str(*work_factors, NULL);
      printf("%s", c_str);
      free(c_str);

      // uint64_t out_size;
      // char *buf = work_factors_factors_to_str(*work_factors, &out_size);
      // printf("%.*s", out_size, buf);
      // free(buf);
    }

    printf("\n");
  } else {
    print_help();
    return 1;
  }

  return 0;
}
