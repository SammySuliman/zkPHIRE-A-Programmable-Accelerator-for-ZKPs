#ifndef ZKPHIRE_SCRATCHPAD_HPP
#define ZKPHIRE_SCRATCHPAD_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// Scratchpad — 16-bank BRAM buffers caching MLE tiles
//
// Paper parity (zkPHIRE §IV): 16 scratchpad banks store MLE table tiles
// for reuse across terms without re-fetching from off-chip memory.
// ---------------------------------------------------------------------------

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
