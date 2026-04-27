#ifndef ZKPHIRE_SCRATCHPAD_HPP
#define ZKPHIRE_SCRATCHPAD_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// Scratchpad buffers — paper-parity (zkPHIRE §IV, 16 banked scratchpads)
//
// PYNQ-Z2 scale: 4 banks, each MAX_TABLE_SIZE/2 × 256 bits.
// Phase 3d: Adds BRAM utilization (previously 0).
// ---------------------------------------------------------------------------

static const int SCRATCHPAD_BANKS = 4;
static const int SCRATCHPAD_DEPTH = MAX_TABLE_SIZE / 2;

// Scratchpad storage — declared in function scope via macros/struct
struct scratchpad_t {
    field_elem_t bank[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH];
};

// Load a tile into scratchpad
static void scratchpad_load(
    field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH],
    int bank_id,
    const field_elem_t* table,
    int tile_size
) {
    load_loop:
    for (int i = 0; i < tile_size; ++i) {
#pragma HLS PIPELINE II=1
        sp[bank_id][i] = table[i];
    }
}

// Read a pair from scratchpad
static void scratchpad_read_pair(
    field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH],
    int bank_id,
    int pair_idx,
    field_elem_t& f0,
    field_elem_t& f1
) {
#pragma HLS INLINE
    f0 = sp[bank_id][2 * pair_idx];
    f1 = sp[bank_id][2 * pair_idx + 1];
}

// Write updated table back to scratchpad
static void scratchpad_write_updated(
    field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH],
    int bank_id,
    const field_elem_t* updated,
    int new_size
) {
    write_loop:
    for (int i = 0; i < new_size; ++i) {
#pragma HLS PIPELINE II=1
        sp[bank_id][i] = updated[i];
    }
}

#endif // ZKPHIRE_SCRATCHPAD_HPP
