#ifndef ZKPHIRE_SCRATCHPAD_HPP
#define ZKPHIRE_SCRATCHPAD_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// Scratchpad buffers — paper-parity (zkPHIRE §IV, 16 banked scratchpads)
//
// PYNQ-Z2 scale: 4 banks, each MAX_TABLE_SIZE/2 × 256 bits.
// Stores MLE tiles for reuse across terms in multi-term polynomials.
// Paper uses scratchpads to avoid re-fetching MLE data from off-chip
// when the same MLE appears in multiple terms.
//
// Phase 3d: Adds BRAM utilization (previously 0).
// ---------------------------------------------------------------------------

static const int SCRATCHPAD_BANKS = 4;
static const int SCRATCHPAD_DEPTH = MAX_TABLE_SIZE / 2;

// Each bank stores one MLE table tile
static field_elem_t scratchpad[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH];
#pragma HLS ARRAY_PARTITION variable=scratchpad complete dim=1
#pragma HLS BIND_STORAGE variable=scratchpad type=RAM_T2P impl=BRAM

// Load a tile into a scratchpad bank
static void scratchpad_load(
    int bank_id,
    const field_elem_t* table,
    int tile_size
) {
    load_loop:
    for (int i = 0; i < tile_size; ++i) {
#pragma HLS PIPELINE II=1
        scratchpad[bank_id][i] = table[i];
    }
}

// Read a pair from a scratchpad bank
static void scratchpad_read_pair(
    int bank_id,
    int pair_idx,
    field_elem_t& f0,
    field_elem_t& f1
) {
#pragma HLS INLINE
    f0 = scratchpad[bank_id][2 * pair_idx];
    f1 = scratchpad[bank_id][2 * pair_idx + 1];
}

// Write updated table back to scratchpad (for next round)
static void scratchpad_write_updated(
    int bank_id,
    const field_elem_t* updated,
    int new_size
) {
    write_loop:
    for (int i = 0; i < new_size; ++i) {
#pragma HLS PIPELINE II=1
        scratchpad[bank_id][i] = updated[i];
    }
}

#endif // ZKPHIRE_SCRATCHPAD_HPP
