#include "work.h"

// Standard library includes.
#include <stdlib.h>
#include <sys/random.h>
#include <stdio.h>

#define VALUE_MAX_DIGITS 1000

void work_cleanup_factors(Work_Factors *factors) {
  if (factors) {
    simple_archiver_chunked_array_cleanup(&factors->value);
    if (factors->factors) {
      simple_archiver_priority_heap_free(&factors->factors);
    }
  }
}

Work_Factors work_generate_target_factors(void) {
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

  while ((first_temp && simple_archiver_chunked_array_size(&temp) < VALUE_MAX_DIGITS)
         || (!first_temp && simple_archiver_chunked_array_size(&temp2) < VALUE_MAX_DIGITS)) {
    simple_archiver_chunked_array_clear(first_temp ? &temp2 : &temp);
    r = rand();
    if (r < 0) {
      r = -r;
    }
    switch (r % 11) {
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

  factors.value = first_temp ? temp : temp2;
  simple_archiver_chunked_array_cleanup(first_temp ? &temp2 : &temp);

  return factors;
}
