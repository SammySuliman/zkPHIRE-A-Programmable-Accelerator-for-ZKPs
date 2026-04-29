#include "include/types.hpp"
#include "include/field_arithmetic.hpp"
#include "include/update_unit.hpp"
#include "include/extension_engine.hpp"
#include "include/product_lane.hpp"
#include "include/accumulator.hpp"
#include "include/scratchpad.hpp"

// ===================================================================
// PE SumCheck Datapath — regular domain, one PE per tile range
//
// Paper parity (zkPHIRE Figure 4):
//   Load → Extension Engine → Product Lane → Accumulator → Update
// ===================================================================

static void pe_sumcheck_round(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree,
    int start_pair,
    int num_pairs,
    field_elem_t r,
    field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH],
    field_elem_t pe_samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
    int deg = degree;
    if (deg > MAX_DEGREE) deg = MAX_DEGREE;

    field_elem_t round_samples[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=round_samples complete dim=1
    accum_init(deg, round_samples);

    // Load tables into scratchpad
    if (start_pair == 0) {
        for (int m = 0; m < deg && m < SCRATCHPAD_BANKS; ++m) {
#pragma HLS UNROLL
            scratchpad_load(sp, m, tables[m], num_pairs * 2);
        }
    }

    // Main pair loop
    pair_loop:
    for (int k = start_pair; k < start_pair + num_pairs; ++k) {
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
        pe_samples[x] = round_samples[x];
    }

    for (int mle_idx = 0; mle_idx < deg; ++mle_idx) {
#pragma HLS PIPELINE II=1
        update_table<MAX_TABLE_SIZE>(tables[mle_idx], r, updated[mle_idx]);
    }
}

// ===================================================================
// Multi-PE Orchestration — 8 PEs via UNROLL + reduction tree
// ===================================================================

static void multi_pe_sumcheck(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree,
    int size,
    field_elem_t r,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
    int deg = degree;
    if (deg > MAX_DEGREE) deg = MAX_DEGREE;
    const int pair_count = size / 2;
    const int pairs_per_pe = pair_count / NUM_PES;

    field_elem_t pe_all_samples[NUM_PES][MAX_SAMPLES];
    field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH];
#pragma HLS ARRAY_PARTITION variable=pe_all_samples complete dim=1
#pragma HLS ARRAY_PARTITION variable=sp complete dim=1

    for (int pe = 0; pe < NUM_PES; ++pe) {
#pragma HLS UNROLL
        int start = pe * pairs_per_pe;
        pe_sumcheck_round(tables, deg, start, pairs_per_pe,
                          r, sp, pe_all_samples[pe], updated);
    }

    // Tree reduction
    field_elem_t combined[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=combined complete dim=1
    for (int x = 0; x <= deg; ++x) {
#pragma HLS PIPELINE II=1
        combined[x] = pe_all_samples[0][x];
    }
    for (int pe = 1; pe < NUM_PES; ++pe) {
#pragma HLS PIPELINE II=1
        for (int x = 0; x <= deg; ++x) {
#pragma HLS UNROLL
            combined[x] = mod_add(combined[x], pe_all_samples[pe][x]);
        }
    }

    for (int x = 0; x <= deg; ++x) {
#pragma HLS PIPELINE II=1
        samples[x] = combined[x];
    }
}

// ===================================================================
// API 1: sumcheck_round_array — BRAM interface
// ===================================================================

status_t sumcheck_round_array(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    field_elem_t r,
    int degree,
    int size,
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

    multi_pe_sumcheck(tables, degree, size, r, samples, updated);

    for (int m = degree; m < MAX_DEGREE; ++m) {
        for (int k = 0; k < size / 2; ++k) {
#pragma HLS PIPELINE
            updated[m][k] = field_elem_t(0);
        }
    }
    return STATUS_OK;
}

// ===================================================================
// API 2: sumcheck_round_axis — AXI-Stream interface
// ===================================================================

status_t sumcheck_round_axis(
    hls::stream<axis_word_t>& in_stream,
    hls::stream<axis_word_t>& out_stream,
    int degree,
    int size,
    field_elem_t r
) {
#pragma HLS INTERFACE axis      port=in_stream
#pragma HLS INTERFACE axis      port=out_stream
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=degree  bundle=control
#pragma HLS INTERFACE s_axilite port=size    bundle=control
#pragma HLS INTERFACE s_axilite port=r       bundle=control
#pragma HLS INTERFACE ap_ctrl_none port=return

    if (degree < 1 || degree > MAX_DEGREE)  return STATUS_BAD_DEGREE;
    if (size < 2 || size > MAX_TABLE_SIZE)  return STATUS_BAD_SIZE;
    if ((size & (size - 1)) != 0)           return STATUS_BAD_SIZE;
    if (r >= FIELD_P)                       return STATUS_BAD_CHALLENGE;

    field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE];
#pragma HLS ARRAY_PARTITION variable=tables complete dim=1

    int deg = degree;
    for (int pair = 0; pair < size / 2; ++pair) {
        for (int mle = 0; mle < deg; ++mle) {
#pragma HLS PIPELINE II=1
            axis_word_t w; w = in_stream.read(); tables[mle][2*pair] = w.data;
            w = in_stream.read(); tables[mle][2*pair+1] = w.data;
        }
    }

    field_elem_t samples[MAX_SAMPLES];
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2];
#pragma HLS ARRAY_PARTITION variable=samples complete dim=1

    multi_pe_sumcheck(tables, deg, size, r, samples, updated);

    for (int x = 0; x <= deg; ++x) {
#pragma HLS PIPELINE II=1
        axis_word_t w; w.data = samples[x]; w.last = (x == deg);
        out_stream.write(w);
    }
    for (int mle = 0; mle < deg; ++mle) {
        for (int k = 0; k < size / 2; ++k) {
#pragma HLS PIPELINE II=1
            axis_word_t w; w.data = updated[mle][k];
            w.last = (mle == deg-1 && k == size/2-1);
            out_stream.write(w);
        }
    }
    return STATUS_OK;
}
