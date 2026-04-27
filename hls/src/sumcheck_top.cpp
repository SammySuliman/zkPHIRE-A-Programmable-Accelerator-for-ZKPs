#include "include/types.hpp"
#include "include/field_arithmetic.hpp"
#include "include/update_unit.hpp"
#include "include/extension_engine.hpp"
#include "include/product_lane.hpp"
#include "include/accumulator.hpp"

// ===================================================================
// Internal one-round SumCheck datapath (hardware-agnostic core)
//
// This is the reusable engine. Both sumcheck_round_array (C-sim /
// BRAM) and sumcheck_round_axi (m_axi / board-ready) delegate to it.
//
// Parameters:
//   tables          — input MLE tables [degree][size], packed as
//                     [degree][MAX_TABLE_SIZE]
//   degree          — number of MLE factors in this product term
//   size            — current table size (power of two, >= 2)
//   r               — verifier challenge
//
// Outputs:
//   samples         — d+1 round polynomial evaluations s(0..d)
//   updated         — halved MLE tables [degree][size/2]
//
// Invariants (verified in testbench against golden model):
//   1. s(0) + s(1) == claim_before
//   2. new_claim == s(r)
//   3. Table size halved exactly
// ===================================================================

static void sumcheck_datapath(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    int degree,
    int size,
    field_elem_t r,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
) {
    // Guard against out-of-range degree
    int deg = degree;
    if (deg > MAX_DEGREE) deg = MAX_DEGREE;
    const int pair_count = size / 2;

    // --- Initialize accumulators ---
    field_elem_t round_samples[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=round_samples complete dim=1
    accum_init(deg, round_samples);

    // --- Main pair loop: extend → multiply → accumulate ---
    pair_loop:
    for (int k = 0; k < pair_count; ++k) {
#pragma HLS PIPELINE II=1

        // Extend each MLE pair to deg+1 evaluations
        field_elem_t extensions[MAX_DEGREE][MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=extensions complete dim=0
        extend_all_for_pair(tables, k, deg, extensions);

        // Per-point product across all MLE factors
        field_elem_t lane_products[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=lane_products complete dim=1
        compute_lane_products(extensions, deg, lane_products);

        // Accumulate into round samples
        accum_add(lane_products, deg, round_samples);
    }

    // --- Write round samples ---
    sample_write:
    for (int x = 0; x <= deg; ++x) {
#pragma HLS UNROLL
        samples[x] = round_samples[x];
    }

    // --- Update each MLE table with verifier challenge r ---
    update_all_loop:
    for (int mle_idx = 0; mle_idx < deg; ++mle_idx) {
#pragma HLS UNROLL
        update_table<MAX_TABLE_SIZE>(tables[mle_idx], r, updated[mle_idx]);
    }
}


// ===================================================================
// API 1: sumcheck_round_array — C-sim / BRAM convenience wrapper
//
// Uses BRAM-partitioned arrays for fast C-simulation.
// This is the recommended API for initial verification and for
// software-only testing without Vitis.
//
// Usage:
//   status_t status = sumcheck_round_array(
//       input_tables, challenge, degree, table_size,
//       samples, updated_tables
//   );
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

    // Validate inputs
    if (degree < 1 || degree > MAX_DEGREE)  return STATUS_BAD_DEGREE;
    if (size < 2 || size > MAX_TABLE_SIZE)  return STATUS_BAD_SIZE;
    if ((size & (size - 1)) != 0)           return STATUS_BAD_SIZE; // power of two
    if (r >= FIELD_P)                       return STATUS_BAD_CHALLENGE;

    sumcheck_datapath(tables, degree, size, r, samples, updated);

    // Zero unused entries in updated tables for determinism
    for (int m = degree; m < MAX_DEGREE; ++m) {
        for (int k = 0; k < size / 2; ++k) {
#pragma HLS PIPELINE
            updated[m][k] = field_elem_t(0);
        }
    }

    return STATUS_OK;
}


// ===================================================================
// API 2: sumcheck_round_axi — m_axi board-ready interface
//
// Uses AXI memory-mapped ports for FPGA/DMA integration.
// The host writes MLE tables to DRAM, triggers the core, and reads
// results back from DRAM.
//
// Port layout:
//   mle_inputs    : gmem0 — input tables [degree][size]
//   round_samples : gmem1 — output round samples s(0..d)
//   next_tables   : gmem2 — output updated tables [degree][size/2]
//
// Usage (host-side):
//   Write MLE tables to mle_inputs buffer
//   Write degree, size, r via s_axilite
//   Start core
//   Wait for done interrupt
//   Read round_samples and next_tables
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

    // Validate inputs
    if (degree < 1 || degree > MAX_DEGREE)  return STATUS_BAD_DEGREE;
    if (size < 2 || size > MAX_TABLE_SIZE)  return STATUS_BAD_SIZE;
    if ((size & (size - 1)) != 0)           return STATUS_BAD_SIZE;
    if (r >= FIELD_P)                       return STATUS_BAD_CHALLENGE;

    // --- Burst-read input tables into local BRAM ---
    field_elem_t local_tables[MAX_DEGREE][MAX_TABLE_SIZE];
#pragma HLS ARRAY_PARTITION variable=local_tables complete dim=1

    load_loop:
    for (int mle = 0; mle < degree; ++mle) {
        for (int i = 0; i < size; ++i) {
#pragma HLS PIPELINE II=1
            local_tables[mle][i] = mle_inputs[mle * size + i];
        }
    }

    // --- Run the datapath ---
    field_elem_t local_samples[MAX_SAMPLES];
    field_elem_t local_updated[MAX_DEGREE][MAX_TABLE_SIZE / 2];
#pragma HLS ARRAY_PARTITION variable=local_samples complete dim=1

    sumcheck_datapath(local_tables, degree, size, r, local_samples, local_updated);

    // --- Burst-write results back to DRAM ---
    store_samples:
    for (int x = 0; x <= degree; ++x) {
#pragma HLS PIPELINE II=1
        round_samples[x] = local_samples[x];
    }

    store_updated:
    for (int mle = 0; mle < degree; ++mle) {
        for (int k = 0; k < size / 2; ++k) {
#pragma HLS PIPELINE II=1
            next_tables[mle * (size / 2) + k] = local_updated[mle][k];
        }
    }

    return STATUS_OK;
}
