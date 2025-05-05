#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_WORK_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_WORK_H_

// Standard library includes.
#include <stdint.h>

// Third party includes.
#include <data_structures/priority_heap.h>

typedef struct Work_Factors {
  SDArchiverChunkedArr *value;
  SDArchiverPHeap *factors;
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

// Returns a string representing the factors. If "len_out" is NULL, then the
// returned string is a C-string. If "len_out" is not NULL, then the length
// of the string will be put in "len_out" and the returned string will NOT be
// NULL terminated.
// 
// The returned buffer must be free'd after use.
char *work_factors_factors_to_str(Work_Factors work_factors, uint64_t *len_out);

#endif
