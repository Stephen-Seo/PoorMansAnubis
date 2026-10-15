#include "random.h"

#include <random>

struct RandomState {
    RandomState();

    std::default_random_engine re;
    std::uniform_int_distribution<int> int_dist;
};

RandomState::RandomState() :
re(std::random_device{}()),
int_dist(0, 10)
{}

void *rand_state_init(void) {
    RandomState *state = new RandomState{};

    return state;
}

void rand_state_cleanup(void *state) {
    RandomState *r_state = reinterpret_cast<RandomState*>(state);

    delete r_state;
}

int rand_int_range(void *state, int min, int max) {
    if (!state) {
        return -1;
    }

    RandomState *r_state = reinterpret_cast<RandomState*>(state);

    decltype(r_state->int_dist)::param_type range(min, max);

    return r_state->int_dist(r_state->re, range);
}
