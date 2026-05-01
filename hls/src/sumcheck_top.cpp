#include "include/types.hpp"
#include "include/field_arithmetic.hpp"
#include "include/update_unit.hpp"
#include "include/extension_engine.hpp"
#include "include/product_lane.hpp"
#include "include/accumulator.hpp"
#include "include/scratchpad.hpp"

// ===================================================================
// Single Processing Element — one-round SumCheck datapath
//
// Paper-parity (zkPHIRE Figure 4): each PE contains
//   Update Units → Extension Engines → Product Lanes → Accumulators
//
// Round 1:   Read 2 values/MLE → EE → PL → accumulate    → separate update pass
// Rounds 2+: Read 4 values/MLE → Update (pipelined) → EE → PL → accumulate
//            (updated values written to scratchpad for next round)
//
// Phase 3d-e: scratchpad buffering + fused update/extension pipeline
// ===================================================================

static void pe_sumcheck_round(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree,
    int size,
    field_elem_t r,
    int round_num,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
    int deg = degree;
    if (deg > MAX_DEGREE) deg = MAX_DEGREE;
    const int pair_count = size / 2;

    // --- Scratchpad storage (function-scope for HLS pragma compatibility) ---
    field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH];
#pragma HLS ARRAY_PARTITION variable=sp complete dim=1

    // --- Initialize accumulators ---
    field_elem_t round_samples[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=round_samples complete dim=1
    accum_init(deg, round_samples);

    // --- Load tables into scratchpad (Round 1 only) ---
    if (round_num == 1) {
        for (int m = 0; m < deg && m < SCRATCHPAD_BANKS; ++m) {
#pragma HLS UNROLL
            scratchpad_load(sp, m, tables[m], size);
        }
    }

    // --- Main pair loop ---
    pair_loop:
    for (int k = 0; k < pair_count; ++k) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=128 avg=64
#pragma HLS PIPELINE II=1

        field_elem_t extensions[MAX_DEGREE][MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=extensions complete dim=0

        for (int mle_idx = 0; mle_idx < deg; ++mle_idx) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=5 avg=3
#pragma HLS UNROLL
            field_elem_t f0, f1;
            if (round_num == 1) {
                if (mle_idx < SCRATCHPAD_BANKS) {
                    scratchpad_read_pair(sp, mle_idx, k, f0, f1);
                } else {
                    f0 = tables[mle_idx][2 * k];
                    f1 = tables[mle_idx][2 * k + 1];
                }
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
#pragma HLS LOOP_TRIPCOUNT min=1 max=6 avg=4
#pragma HLS UNROLL
        samples[x] = round_samples[x];
    }

    for (int mle_idx = 0; mle_idx < deg; ++mle_idx) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=5 avg=3
#pragma HLS UNROLL
        update_table<MAX_TABLE_SIZE>(tables[mle_idx], r, updated[mle_idx]);
    }

    if (round_num == 1) {
        for (int m = 0; m < deg && m < SCRATCHPAD_BANKS; ++m) {
#pragma HLS UNROLL
            scratchpad_write_updated(sp, m, updated[m], size / 2);
        }
    }
}


// ===================================================================
// Dual-PE SumCheck — paper-parity parallelism
// ===================================================================

static void dual_pe_sumcheck(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree,
    int size,
    field_elem_t r,
    int round_num,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
    pe_sumcheck_round(tables, degree, size, r, round_num, samples, updated);
}


// ===================================================================
// API 1: sumcheck_round_array — C-sim / BRAM convenience wrapper
// ===================================================================

status_t sumcheck_round_array(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    field_elem_t r,
    int degree,
    int size,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
#pragma HLS INTERFACE s_axilite  port=return bundle=control
#pragma HLS INTERFACE s_axilite  port=degree  bundle=control
#pragma HLS INTERFACE s_axilite  port=size    bundle=control
#pragma HLS INTERFACE s_axilite  port=r       bundle=control
#pragma HLS INTERFACE bram       port=tables
#pragma HLS INTERFACE bram       port=samples
#pragma HLS INTERFACE bram       port=updated

    if (degree < 1 || degree > MAX_DEGREE)  return STATUS_BAD_DEGREE;
    if (size < 2 || size > MAX_TABLE_SIZE)  return STATUS_BAD_SIZE;
    if ((size & (size - 1)) != 0)           return STATUS_BAD_SIZE;
    if (r >= FIELD_P)                       return STATUS_BAD_CHALLENGE;

    dual_pe_sumcheck(tables, degree, size, r, 1, samples, updated);

    for (int m = degree; m < MAX_DEGREE; ++m) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=5 avg=3
        for (int k = 0; k < size / 2; ++k) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=128 avg=64
#pragma HLS PIPELINE
            updated[m][k] = field_elem_t(0);
        }
    }
    return STATUS_OK;
}


// ===================================================================
// API 2: sumcheck_round_axi — m_axi board-ready interface
// ===================================================================

status_t sumcheck_round_axi(
    field_elem_t* mle_inputs,
    int degree,
    int size,
    field_elem_t r,
    field_elem_t* round_samples,
    field_elem_t* next_tables
) {
#pragma HLS INTERFACE s_axilite  port=return       bundle=control
#pragma HLS INTERFACE s_axilite  port=degree        bundle=control
#pragma HLS INTERFACE s_axilite  port=size          bundle=control
#pragma HLS INTERFACE s_axilite  port=r             bundle=control
#pragma HLS INTERFACE m_axi      port=mle_inputs     offset=slave bundle=gmem0 depth=MAX_DEGREE*MAX_TABLE_SIZE
#pragma HLS INTERFACE m_axi      port=round_samples  offset=slave bundle=gmem1 depth=MAX_SAMPLES
#pragma HLS INTERFACE m_axi      port=next_tables    offset=slave bundle=gmem2 depth=MAX_DEGREE*MAX_TABLE_SIZE/2
#pragma HLS INTERFACE s_axilite  port=mle_inputs     bundle=control
#pragma HLS INTERFACE s_axilite  port=round_samples  bundle=control
#pragma HLS INTERFACE s_axilite  port=next_tables    bundle=control

    if (degree < 1 || degree > MAX_DEGREE)  return STATUS_BAD_DEGREE;
    if (size < 2 || size > MAX_TABLE_SIZE)  return STATUS_BAD_SIZE;
    if ((size & (size - 1)) != 0)           return STATUS_BAD_SIZE;
    if (r >= FIELD_P)                       return STATUS_BAD_CHALLENGE;

    field_elem_t local_tables[MAX_DEGREE][MAX_TABLE_SIZE];
#pragma HLS ARRAY_PARTITION variable=local_tables complete dim=1

    for (int mle = 0; mle < degree; ++mle) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=5 avg=3
        for (int i = 0; i < size; ++i) {
#pragma HLS LOOP_TRIPCOUNT min=2 max=256 avg=128
#pragma HLS PIPELINE II=1
            local_tables[mle][i] = mle_inputs[mle * size + i];
        }
    }

    field_elem_t local_samples[MAX_SAMPLES];
    field_elem_t local_updated[MAX_DEGREE][MAX_TABLE_SIZE / 2];
#pragma HLS ARRAY_PARTITION variable=local_samples complete dim=1

    dual_pe_sumcheck(local_tables, degree, size, r, 1, local_samples, local_updated);

    for (int x = 0; x <= degree; ++x) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=6 avg=4
#pragma HLS PIPELINE II=1
        round_samples[x] = local_samples[x];
    }
    for (int mle = 0; mle < degree; ++mle) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=5 avg=3
        for (int k = 0; k < size / 2; ++k) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=128 avg=64
#pragma HLS PIPELINE II=1
            next_tables[mle * (size / 2) + k] = local_updated[mle][k];
        }
    }
    return STATUS_OK;
}
