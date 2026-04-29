#ifndef ZKPHIRE_ACCUMULATOR_HPP
#define ZKPHIRE_ACCUMULATOR_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

static void accum_init(int degree, field_elem_t round_samples[MAX_SAMPLES]) {
    init_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS PIPELINE II=1
        round_samples[x] = field_elem_t(0);
    }
}

static void accum_add(
    field_elem_t lane_products[MAX_SAMPLES],
    int degree,
    field_elem_t round_samples[MAX_SAMPLES]
) {
#pragma HLS INLINE
    acc_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS PIPELINE II=1
        round_samples[x] = mod_add(round_samples[x], lane_products[x]);
    }
}

#endif
