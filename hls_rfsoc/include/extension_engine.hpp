#ifndef ZKPHIRE_EXTENSION_ENGINE_HPP
#define ZKPHIRE_EXTENSION_ENGINE_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

// ---------------------------------------------------------------------------
// Extension Engine — extend MLE pair to d+1 point evaluations
//
// For a pair (f0, f1) and degree d, produce values at x = 0, 1, ..., d
// using the affine rule: eval_at_x = f0 + (f1-f0) * x
// All values in Montgomery domain.
// ---------------------------------------------------------------------------

static void extend_pair(
    field_elem_t f0,
    field_elem_t f1,
    int degree,
    field_elem_t out[MAX_SAMPLES]
) {
#pragma HLS INLINE
    field_elem_t diff = mod_sub(f1, f0);

    extend_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS PIPELINE II=1
        field_elem_t z = to_montgomery(field_elem_t(x));
        out[x] = mod_add(f0, mod_mul(diff, z));
    }
}

// Extend all MLE tables for one pair index
static void extend_all_for_pair(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int pair_idx,
    int degree,
    field_elem_t extensions[MAX_DEGREE][MAX_SAMPLES]
) {
    mle_loop:
    for (int mle_idx = 0; mle_idx < degree; ++mle_idx) {
#pragma HLS PIPELINE II=1
        field_elem_t f0 = tables[mle_idx][2 * pair_idx];
        field_elem_t f1 = tables[mle_idx][2 * pair_idx + 1];
        extend_pair(f0, f1, degree, extensions[mle_idx]);
    }
}

#endif // ZKPHIRE_EXTENSION_ENGINE_HPP
