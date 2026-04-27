#ifndef ZKPHIRE_ACCUMULATOR_HPP
#define ZKPHIRE_ACCUMULATOR_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

// ---------------------------------------------------------------------------
// accumulator: accumulate per-pair products into the round-sample registers
//
// For each pair index k, after computing lane_products[x] for x in 0..d,
// accumulate into the d+1 round-sample registers:
//   round_samples[x] += lane_products[x]
//
// The accumulator is initialized to zero before the first pair is processed.
// After all pairs are consumed, round_samples holds s(0..d).
// ---------------------------------------------------------------------------

// Initialize round-sample accumulators to zero
static void accum_init(
    int degree,
    field_elem_t round_samples[MAX_SAMPLES]
) {
    init_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS UNROLL
        round_samples[x] = field_elem_t(0);
    }
}

// Accumulate one set of per-point lane products into the round samples
static void accum_add(
    field_elem_t lane_products[MAX_SAMPLES],
    int degree,
    field_elem_t round_samples[MAX_SAMPLES]
) {
#pragma HLS INLINE
    acc_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS UNROLL
        round_samples[x] = mod_add(round_samples[x], lane_products[x]);
    }
}

#endif // ZKPHIRE_ACCUMULATOR_HPP
