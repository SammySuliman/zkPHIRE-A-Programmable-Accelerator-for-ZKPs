#ifndef ZKPHIRE_EXTENSION_ENGINE_HPP
#define ZKPHIRE_EXTENSION_ENGINE_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// extension_engine: extend one MLE pair to degree+1 point evaluations
//
// For a pair (f0, f1) and configured degree d:
//   extend_pair(f0, f1, d) -> [value at x=0, 1, ..., d]
//
// Each evaluation uses the shared affine rule:
//   value_at_x = f0*(1-x) + f1*x = f0 + (f1-f0)*x
//
// The extension uses integer points 0..d (not the verifier challenge).
// As d <= MAX_DEGREE (=6) for the first milestone, we unroll completely.
// ---------------------------------------------------------------------------

static void extend_pair(
    field_elem_t f0,
    field_elem_t f1,
    int degree,
    field_elem_t out[MAX_DEGREE + 1]
) {
#pragma HLS INLINE
    field_elem_t diff = mod_sub(f1, f0);

    // Unrolled: degree <= MAX_DEGREE ensures bounded loop
    extend_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS UNROLL
        out[x] = mod_add(f0, mod_mul(diff, field_elem_t(x)));
    }
}

// ---------------------------------------------------------------------------
// Extend all MLE tables' current pair:
//   all_extensions[mle_idx][pair_idx][x] = eval at x for that table's pair
//
// This produces degree parallel extensions for each of 'degree' MLE tables,
// one pair at a time.
// ---------------------------------------------------------------------------

static void extend_all_for_pair(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int pair_idx,
    int degree,
    field_elem_t extensions[MAX_DEGREE][MAX_DEGREE + 1]
) {
    // For each MLE table, extend its current pair
    mle_loop:
    for (int mle_idx = 0; mle_idx < degree; ++mle_idx) {
#pragma HLS UNROLL
        const field_elem_t* table = tables[mle_idx];
        field_elem_t f0 = table[2 * pair_idx];
        field_elem_t f1 = table[2 * pair_idx + 1];
        extend_pair(f0, f1, degree, extensions[mle_idx]);
    }
}

#endif // ZKPHIRE_EXTENSION_ENGINE_HPP
