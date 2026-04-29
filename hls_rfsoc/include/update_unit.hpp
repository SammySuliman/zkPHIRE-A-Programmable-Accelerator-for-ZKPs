#ifndef ZKPHIRE_UPDATE_UNIT_HPP
#define ZKPHIRE_UPDATE_UNIT_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

// ---------------------------------------------------------------------------
// Update Unit — MLE update with verifier challenge r
//
// Paper parity (zkPHIRE Figure 4):
//   Round 1:  update2 — standard 2-input MLE update
//   Rounds 2+: update4 — pipelined 4-input update feeding Extension Engine
// ---------------------------------------------------------------------------

// Standard 2-input update: f0, f1 → updated value = f0 + (f1-f0)*r
static field_elem_t update2(field_elem_t f0, field_elem_t f1, field_elem_t r) {
#pragma HLS INLINE
    return affine_line_eval(f0, f1, r);
}

// Pipelined 4-input update (Rounds 2+): produces two updated values
static void update4(
    field_elem_t f00, field_elem_t f01,
    field_elem_t f10, field_elem_t f11,
    field_elem_t r,
    field_elem_t& next0,
    field_elem_t& next1
) {
#pragma HLS INLINE
    next0 = affine_line_eval(f00, f01, r);
    next1 = affine_line_eval(f10, f11, r);
}

// Apply 2-input update to entire MLE table, producing halved table
template <int SIZE>
static void update_table(
    const field_elem_t table[SIZE],
    field_elem_t r,
    field_elem_t out[SIZE / 2]
) {
    update_loop:
    for (int k = 0; k < SIZE / 2; ++k) {
#pragma HLS PIPELINE II=1
        out[k] = update2(table[2 * k], table[2 * k + 1], r);
    }
}

#endif // ZKPHIRE_UPDATE_UNIT_HPP
