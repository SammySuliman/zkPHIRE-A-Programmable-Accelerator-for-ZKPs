#ifndef ZKPHIRE_PRODUCT_LANE_HPP
#define ZKPHIRE_PRODUCT_LANE_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

// ---------------------------------------------------------------------------
// product_lane: form the per-point product across all MLE extension factors
//
// Phase 3: uses PIPELINE instead of UNROLL so that the single shared
// modular multiplier (via HLS ALLOCATION) is reused across cycles.
// The MLE factor loop is pipelined; the point loop is unrolled (d ≤ 6).
// ---------------------------------------------------------------------------

static void compute_lane_products(
    field_elem_t extensions[MAX_DEGREE][MAX_SAMPLES],
    int degree,
    field_elem_t lane_products[MAX_SAMPLES]
) {
    // Initialize lane products to 1
    init_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS UNROLL
        lane_products[x] = field_elem_t(1);
    }

    // Multiply across MLE tables for each evaluation point
    mle_loop:
    for (int mle_idx = 0; mle_idx < degree; ++mle_idx) {
#pragma HLS PIPELINE II=1
        point_loop:
        for (int x = 0; x <= degree; ++x) {
#pragma HLS UNROLL
            lane_products[x] = mod_mul(lane_products[x], extensions[mle_idx][x]);
        }
    }
}

#endif // ZKPHIRE_PRODUCT_LANE_HPP
