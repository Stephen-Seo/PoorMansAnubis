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
    for (size_t idx = simple_archiver_chunked_array_size(&work_factors->value);
         idx-- > 0;) {
      printf("%hu",
             *((uint16_t*)simple_archiver_chunked_array_at(&work_factors->value,
                                                           idx)));
    }
    printf("\n");

    int_fast8_t first = 1;
    while (simple_archiver_priority_heap_size(work_factors->factors) != 0) {
      uint16_t *cptr = simple_archiver_priority_heap_pop(work_factors->factors);
      if (first) {
        printf("%hu", *cptr);
        first = 0;
      } else {
        printf(" %hu", *cptr);
      }
      free(cptr);
    }

    printf("\n");
  } else {
    print_help();
    return 1;
  }

  return 0;
}
