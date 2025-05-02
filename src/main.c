// Standard library includes.
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>

// Local includes.
#include "work.h"

int main(int argc, char **argv) {
  __attribute__((cleanup(work_cleanup_factors)))
  Work_Factors work_factors = work_generate_target_factors();

  printf("%" PRIu64 "\n", work_factors.value);

  int_fast8_t first = 1;
  while (simple_archiver_priority_heap_size(work_factors.factors) != 0) {
    uint64_t *u64p = simple_archiver_priority_heap_pop(work_factors.factors);
    if (first) {
      printf("%" PRIu64, *u64p);
      first = 0;
    } else {
      printf(" %" PRIu64, *u64p);
    }
    free(u64p);
  }

  printf("\n");

  return 0;
}
