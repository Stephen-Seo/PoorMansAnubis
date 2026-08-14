#include "random.h"

#include <random>

struct RandomState {
    RandomState();

    std::default_random_engine re;
};

RandomState::RandomState() :
re(std::random_device{}())
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

    return std::uniform_int_distribution<int>(min, max)(r_state->re);
}
