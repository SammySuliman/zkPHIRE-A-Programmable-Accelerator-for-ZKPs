#include "include/types.hpp"
#include "include/field_arithmetic.hpp"
#include "include/update_unit.hpp"
#include "include/extension_engine.hpp"
#include "include/product_lane.hpp"
#include "include/accumulator.hpp"
#include "include/scratchpad.hpp"

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

    field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH];
#pragma HLS ARRAY_PARTITION variable=sp complete dim=1

    // Load tables into scratchpad
    for (int m = 0; m < deg && m < SCRATCHPAD_BANKS; ++m) {
#pragma HLS PIPELINE II=1
        scratchpad_load(sp, m, tables[m], size);
    }

    field_elem_t round_samples[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=round_samples complete dim=1
    accum_init(deg, round_samples);

    pair_loop:
    for (int k = 0; k < pair_count; ++k) {
#pragma HLS PIPELINE II=1
        field_elem_t extensions[MAX_DEGREE][MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=extensions complete dim=0
        for (int mle_idx = 0; mle_idx < deg; ++mle_idx) {
#pragma HLS UNROLL
            field_elem_t f0, f1;
            if (mle_idx < SCRATCHPAD_BANKS) {
                scratchpad_read_pair(sp, mle_idx, k, f0, f1);
            } else {
                f0 = tables[mle_idx][2 * k];
                f1 = tables[mle_idx][2 * k + 1];
            }
            extend_pair(f0, f1, deg, extensions[mle_idx]);
        }
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
