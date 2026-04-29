#ifndef ZKPHIRE_PRODUCT_LANE_HPP
#define ZKPHIRE_PRODUCT_LANE_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

static void compute_lane_products(
    field_elem_t extensions[MAX_DEGREE][MAX_SAMPLES],
    int degree,
    field_elem_t lane_products[MAX_SAMPLES]
) {
    init_loop:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS PIPELINE II=1
        lane_products[x] = field_elem_t(1);
    }
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

#endif
