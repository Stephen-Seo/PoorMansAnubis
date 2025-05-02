#include "work.h"
#include "data_structures/priority_heap.h"

// Standard library includes.
#include <stdlib.h>

void work_cleanup_factors(Work_Factors *factors) {
  if (factors) {
    if (factors->factors) {
      simple_archiver_priority_heap_free(&factors->factors);
    }
  }
}

Work_Factors work_generate_target_factors(void) {
  Work_Factors factors;

  factors.factors = simple_archiver_priority_heap_init();

  factors.value = 1;
  int r;
  uint64_t *temp_u64;
  while (factors.value < 0xFF00000000000000) {
    r = rand();
    if (r < 0) {
      r = -r;
    }
    switch (r % 13) {
      case 0:
        factors.value *= 2;
        temp_u64 = malloc(8);
        *temp_u64 = 2;
        simple_archiver_priority_heap_insert(factors.factors,
                                             2,
                                             temp_u64,
                                             NULL);
        break;
      case 1:
        factors.value *= 3;
        temp_u64 = malloc(8);
        *temp_u64 = 3;
        simple_archiver_priority_heap_insert(factors.factors,
                                             3,
                                             temp_u64,
                                             NULL);
        break;
      case 2:
        factors.value *= 5;
        temp_u64 = malloc(8);
        *temp_u64 = 5;
        simple_archiver_priority_heap_insert(factors.factors,
                                             5,
                                             temp_u64,
                                             NULL);
        break;
      case 3:
        factors.value *= 7;
        temp_u64 = malloc(8);
        *temp_u64 = 7;
        simple_archiver_priority_heap_insert(factors.factors,
                                             7,
                                             temp_u64,
                                             NULL);
        break;
      case 4:
        factors.value *= 11;
        temp_u64 = malloc(8);
        *temp_u64 = 11;
        simple_archiver_priority_heap_insert(factors.factors,
                                             11,
                                             temp_u64,
                                             NULL);
        break;
      case 5:
        factors.value *= 13;
        temp_u64 = malloc(8);
        *temp_u64 = 13;
        simple_archiver_priority_heap_insert(factors.factors,
                                             13,
                                             temp_u64,
                                             NULL);
        break;
      case 6:
        factors.value *= 17;
        temp_u64 = malloc(8);
        *temp_u64 = 17;
        simple_archiver_priority_heap_insert(factors.factors,
                                             17,
                                             temp_u64,
                                             NULL);
        break;
      case 7:
        factors.value *= 19;
        temp_u64 = malloc(8);
        *temp_u64 = 19;
        simple_archiver_priority_heap_insert(factors.factors,
                                             19,
                                             temp_u64,
                                             NULL);
        break;
      case 8:
        factors.value *= 23;
        temp_u64 = malloc(8);
        *temp_u64 = 23;
        simple_archiver_priority_heap_insert(factors.factors,
                                             23,
                                             temp_u64,
                                             NULL);
        break;
      case 9:
        factors.value *= 29;
        temp_u64 = malloc(8);
        *temp_u64 = 29;
        simple_archiver_priority_heap_insert(factors.factors,
                                             29,
                                             temp_u64,
                                             NULL);
        break;
      case 10:
        factors.value *= 31;
        temp_u64 = malloc(8);
        *temp_u64 = 31;
        simple_archiver_priority_heap_insert(factors.factors,
                                             31,
                                             temp_u64,
                                             NULL);
        break;
      case 11:
        factors.value *= 37;
        temp_u64 = malloc(8);
        *temp_u64 = 37;
        simple_archiver_priority_heap_insert(factors.factors,
                                             37,
                                             temp_u64,
                                             NULL);
        break;
      case 12:
        factors.value *= 41;
        temp_u64 = malloc(8);
        *temp_u64 = 41;
        simple_archiver_priority_heap_insert(factors.factors,
                                             41,
                                             temp_u64,
                                             NULL);
        break;
    }
  }

  return factors;
}
