#ifndef ZKPHIRE_UPDATE_UNIT_HPP
#define ZKPHIRE_UPDATE_UNIT_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

// ---------------------------------------------------------------------------
// update_unit: affine MLE update at the verifier challenge r
//
// Paper-parity (zkPHIRE Figure 4): two update modes:
//   Round 1:  update2(f0, f1, r) — standard 2-input update
//   Round 2+: update4(f0_0, f0_1, f1_0, f1_1, r) — pipelined 4-input
//             update that produces two updated values while feeding
//             Extension Engines simultaneously.
//
// The 4-input mode implements:
//   next0 = f0_0 * (1-r) + f0_1 * r    (from pair at X_i=0)
//   next1 = f1_0 * (1-r) + f1_1 * r    (from pair at X_i=1)
//
// This folds MLE update into the extension pipeline, eliminating the
// separate update pass in rounds 2+.
// ---------------------------------------------------------------------------

// Standard 2-input update (Round 1)
static field_elem_t update2(field_elem_t f0, field_elem_t f1, field_elem_t r) {
#pragma HLS INLINE
    return affine_line_eval(f0, f1, r);
}

// Pipelined 4-input update (Rounds 2+): produces two updated values
static void update4(
    field_elem_t f0_0, field_elem_t f0_1,
    field_elem_t f1_0, field_elem_t f1_1,
    field_elem_t r,
    field_elem_t& next0,
    field_elem_t& next1
) {
#pragma HLS INLINE
    next0 = affine_line_eval(f0_0, f0_1, r);
    next1 = affine_line_eval(f1_0, f1_1, r);
}

// Apply 2-input update to an entire MLE table
template <int SIZE>
static void update_table(
    const field_elem_t table[SIZE],
    field_elem_t r,
    field_elem_t out[SIZE / 2]
) {
    update_loop:
    for (int k = 0; k < SIZE / 2; ++k) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=128 avg=64
#pragma HLS PIPELINE II=1
        out[k] = update2(table[2 * k], table[2 * k + 1], r);
    }
}

#endif // ZKPHIRE_UPDATE_UNIT_HPP
