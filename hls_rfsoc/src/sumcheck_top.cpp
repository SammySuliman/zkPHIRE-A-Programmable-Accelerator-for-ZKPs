#include "include/types.hpp"
#include "include/field_arithmetic.hpp"
#include "include/update_unit.hpp"
#include "include/extension_engine.hpp"
#include "include/product_lane.hpp"
#include "include/accumulator.hpp"

// ===================================================================
// Single-PE SumCheck — minimal working core for debugging
// ===================================================================

status_t sumcheck_round_array(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    field_elem_t r, int degree, int size,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=degree  bundle=control
#pragma HLS INTERFACE s_axilite port=size    bundle=control
#pragma HLS INTERFACE s_axilite port=r       bundle=control
#pragma HLS INTERFACE bram      port=tables
#pragma HLS INTERFACE bram      port=samples
#pragma HLS INTERFACE bram      port=updated

    if (degree < 1 || degree > MAX_DEGREE)  return STATUS_BAD_DEGREE;
    if (size < 2 || size > MAX_TABLE_SIZE)  return STATUS_BAD_SIZE;
    if ((size & (size - 1)) != 0)           return STATUS_BAD_SIZE;
    if (r >= FIELD_P)                       return STATUS_BAD_CHALLENGE;

    int deg = degree;
    if (deg > MAX_DEGREE) deg = MAX_DEGREE;
    const int pair_count = size / 2;

    field_elem_t round_samples[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=round_samples complete dim=1
    accum_init(deg, round_samples);

    pair_loop:
    for (int k = 0; k < pair_count; ++k) {
#pragma HLS PIPELINE II=1
        field_elem_t extensions[MAX_DEGREE][MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=extensions complete dim=0
        extend_all_for_pair(tables, k, deg, extensions);

        field_elem_t lane_products[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=lane_products complete dim=1
        compute_lane_products(extensions, deg, lane_products);
        accum_add(lane_products, deg, round_samples);
    }

    for (int x = 0; x <= deg; ++x) {
#pragma HLS PIPELINE II=1
        samples[x] = round_samples[x];
    }

    for (int mle_idx = 0; mle_idx < deg; ++mle_idx) {
#pragma HLS PIPELINE II=1
        update_table<MAX_TABLE_SIZE>(tables[mle_idx], r, updated[mle_idx]);
    }

    for (int m = degree; m < MAX_DEGREE; ++m)
        for (int k = 0; k < pair_count; ++k)
#pragma HLS PIPELINE
            updated[m][k] = field_elem_t(0);
    return STATUS_OK;
}
