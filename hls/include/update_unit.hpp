#ifndef ZKPHIRE_UPDATE_UNIT_HPP
#define ZKPHIRE_UPDATE_UNIT_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

// ---------------------------------------------------------------------------
// update_unit: affine MLE update at the verifier challenge r
//
// For one pair (f0, f1) from a constituent MLE table:
//   update(f0, f1, r) = f0*(1-r) + f1*r
//
// Uses the shared affine_line_eval from field_arithmetic.hpp.
// ---------------------------------------------------------------------------

static field_elem_t update(field_elem_t f0, field_elem_t f1, field_elem_t r) {
#pragma HLS INLINE
    return affine_line_eval(f0, f1, r);
}

// Apply update to an entire MLE table, producing a halved table.
// Input:  table[0..size-1]  (size must be even)
// Output: out[0..size/2-1]  (each entry = update(table[2k], table[2k+1], r))
template <int SIZE>
static void update_table(
    const field_elem_t table[SIZE],
    field_elem_t r,
    field_elem_t out[SIZE / 2]
) {
    update_loop:
    for (int k = 0; k < SIZE / 2; ++k) {
#pragma HLS PIPELINE II=1
        out[k] = update(table[2 * k], table[2 * k + 1], r);
    }
}

#endif // ZKPHIRE_UPDATE_UNIT_HPP
