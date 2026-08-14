#ifndef COM_SEODISPARATE_POOR_MANS_ANUBIS_CHALLENGE_RANDOM_H_
#define COM_SEODISPARATE_POOR_MANS_ANUBIS_CHALLENGE_RANDOM_H_

#ifdef __cplusplus
extern "C" {
#endif

void *rand_state_init(void);
void rand_state_cleanup(void *state);

// Returns -1 on error.
int rand_int_range(void *state, int min, int max);

#ifdef __cplusplus
}
#endif

#endif
