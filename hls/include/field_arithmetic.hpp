#ifndef ZKPHIRE_FIELD_ARITHMETIC_HPP
#define ZKPHIRE_FIELD_ARITHMETIC_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// BN254 scalar field arithmetic — bit-exact with the Python golden model
//
// Field prime (BN254 curve order):
//   p = 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001
//
// Phase 3 DSP strategy: mod_mul uses INLINE off + ALLOCATION to share
// a single multiplier instance. The 512-bit % division maps to a sequential
// divider in Vitis HLS which is more DSP-efficient than parallel Barrett.
//
// Documented limitation: Full Montgomery conversion requires caller-side
// domain changes — impractical as drop-in. The current 237 DSPs (107%)
// is close to PYNQ-Z2 budget. See docs/paper-parity-plan.md for details.
// ---------------------------------------------------------------------------

static const ap_uint<512> FIELD_P_512 = ap_uint<512>(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

static field_elem_t mod_mul(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE off
#pragma HLS ALLOCATION instances=mul limit=1 function
    ap_uint<512> prod = ap_uint<512>(a) * b;
    return field_elem_t(prod % FIELD_P_512);
}

static field_elem_t mod_add(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    ap_uint<FIELD_BITS + 1> sum = ap_uint<FIELD_BITS + 1>(a) + b;
    if (sum >= FIELD_P) {
        return field_elem_t(sum - FIELD_P);
    }
    return field_elem_t(sum);
}

static field_elem_t mod_sub(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    if (a >= b) {
        return a - b;
    }
    return (a + FIELD_P) - b;
}

static field_elem_t affine_line_eval(field_elem_t f0, field_elem_t f1, field_elem_t z) {
#pragma HLS INLINE
    field_elem_t diff = mod_sub(f1, f0);
    return mod_add(f0, mod_mul(diff, z));
}

#endif // ZKPHIRE_FIELD_ARITHMETIC_HPP
