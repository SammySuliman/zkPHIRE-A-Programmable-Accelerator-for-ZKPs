#include "include/types.hpp"
#include "include/update_unit.hpp"
#include "include/extension_engine.hpp"
#include "include/product_lane.hpp"
#include "include/accumulator.hpp"

// ---------------------------------------------------------------------------
// sumcheck_top: one-round SumCheck core for a single product term
//
// This implements Section 5 of the SPEC exactly:
//   1. Read pairs from each constituent MLE table
//   2. Extend each pair to d+1 evaluations (extension_engine)
//   3. Multiply per-factor extensions pointwise (product_lane)
//   4. Accumulate products into d+1 round samples (accumulator)
//   5. Update each MLE table with verifier challenge r (update_unit)
//
// Parameters:
//   tables    — input MLE tables, indexed tables[mle_idx][entry]
//               [degree][MAX_TABLE_SIZE], only first 'size' entries valid
//   degree    — number of MLE factors in this product term
//   size      — current table size (power of two, >= 2)
//   r         — verifier challenge on [0, FIELD_P)
//   samples   — output: d+1 round polynomial evaluations s(0..d)
//   updated   — output: halved MLE tables
//               updated[mle_idx][0..size/2-1]
//
// Invariants enforced (checked in testbench against golden model):
//   1. s(0) + s(1) == claim_before (sum of products over full table)
//   2. New claim == s(r)
//   3. Table size is halved exactly
//   4. Samples interpolate to s(r) at any r
// ---------------------------------------------------------------------------

void sumcheck_top(
    // MLE tables — degree × size, packed as degree arrays of MAX_TABLE_SIZE
    field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],

    // Configuration
    int degree,
    int size,
    field_elem_t r,

    // Outputs
    field_elem_t samples[MAX_DEGREE + 1],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=degree  bundle=control
#pragma HLS INTERFACE s_axilite port=size    bundle=control
#pragma HLS INTERFACE s_axilite port=r       bundle=control
#pragma HLS INTERFACE bram    port=tables
#pragma HLS INTERFACE bram    port=samples
#pragma HLS INTERFACE bram    port=updated

    // Guard against out-of-range degree
    int deg = degree;
    if (deg > MAX_DEGREE) {
        deg = MAX_DEGREE;
    }
    const int pair_count = size / 2;

    // -------------------------------------------------------------------
    // Step 0: initialize round-sample accumulators to zero
    // -------------------------------------------------------------------
    field_elem_t round_samples[MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=round_samples complete dim=1
    accum_init(deg, round_samples);

    // -------------------------------------------------------------------
    // Steps 1-4: pair loop — the main SumCheck reduction
    // For each pair (2k, 2k+1) across all MLE tables:
    //   extend -> multiply -> accumulate
    // -------------------------------------------------------------------
    pair_loop:
    for (int k = 0; k < pair_count; ++k) {
#pragma HLS PIPELINE II=1
        // Step 1+2: extend each MLE pair to deg+1 evaluations
        field_elem_t extensions[MAX_DEGREE][MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=extensions complete dim=0
        extend_all_for_pair(tables, k, deg, extensions);

        // Step 3: form per-point product across all MLE factors
        field_elem_t lane_products[MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=lane_products complete dim=1
        compute_lane_products(extensions, deg, lane_products);

        // Step 4: accumulate into round_samples
        accum_add(lane_products, deg, round_samples);
    }

    // Write round samples
    sample_write:
    for (int x = 0; x <= deg; ++x) {
#pragma HLS UNROLL
        samples[x] = round_samples[x];
    }

    // -------------------------------------------------------------------
    // Step 5: update each MLE table with the verifier challenge r
    // -------------------------------------------------------------------
    update_all_loop:
    for (int mle_idx = 0; mle_idx < deg; ++mle_idx) {
#pragma HLS UNROLL
        update_table<MAX_TABLE_SIZE>(tables[mle_idx], r, updated[mle_idx]);
    }

    // Zero out unused table entries for determinism
    unused_zero_loop:
    for (int mle_idx = deg; mle_idx < MAX_DEGREE; ++mle_idx) {
        for (int i = 0; i < pair_count; ++i) {
#pragma HLS PIPELINE
            updated[mle_idx][i] = field_elem_t(0);
        }
    }
}
