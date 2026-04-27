#ifndef ZKPHIRE_PRODUCT_LANE_HPP
#define ZKPHIRE_PRODUCT_LANE_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// product_lane: form the per-point product across all MLE extension factors
//
// For one pair and one evaluation point x in {0..d}:
//   product[x] = prod_{mle_idx=0}^{degree-1} extensions[mle_idx][x]
//
// This is the per-term product that zkPHIRE computes in each product lane.
// ---------------------------------------------------------------------------

static void compute_lane_products(
    field_elem_t extensions[MAX_DEGREE][MAX_DEGREE + 1],
    int degree,
    field_elem_t lane_products[MAX_DEGREE + 1]
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
#pragma HLS UNROLL
        point_loop:
        for (int x = 0; x <= degree; ++x) {
#pragma HLS UNROLL
            lane_products[x] = mod_mul(lane_products[x], extensions[mle_idx][x]);
        }
    }
}

#endif // ZKPHIRE_PRODUCT_LANE_HPP
