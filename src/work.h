#ifndef SEODISPARATE_COM_POOR_MANS_ANUBIS_WORK_H_
#define SEODISPARATE_COM_POOR_MANS_ANUBIS_WORK_H_

// Standard library includes.
#include <stdint.h>

// Third party includes.
#include <data_structures/priority_heap.h>

typedef struct Work_Factors {
  SDArchiverChunkedArr value;
  SDArchiverPHeap *factors;
} Work_Factors;

void work_cleanup_factors(Work_Factors *factors);

Work_Factors work_generate_target_factors(void);

#endif
