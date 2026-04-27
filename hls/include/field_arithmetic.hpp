#ifndef ZKPHIRE_FIELD_ARITHMETIC_HPP
#define ZKPHIRE_FIELD_ARITHMETIC_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// BN254 scalar field arithmetic — bit-exact with the Python golden model
//
// Field prime (BN254 curve order):
//   p = 21888242871839275222246405745257275088548364400416034343698204186575808495617
//     = 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001
// ---------------------------------------------------------------------------

// FIELD_P is declared in types.hpp; we store a 512-bit version
// for multiplication reduction.
static const ap_uint<512> FIELD_P_512 = ap_uint<512>(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

// ---------------------------------------------------------------------------
// Modular addition: (a + b) mod p
// ---------------------------------------------------------------------------
static field_elem_t mod_add(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    ap_uint<FIELD_BITS + 1> sum = ap_uint<FIELD_BITS + 1>(a) + b;
    if (sum >= FIELD_P) {
        return field_elem_t(sum - FIELD_P);
    }
    return field_elem_t(sum);
}

// ---------------------------------------------------------------------------
// Modular subtraction: (a - b) mod p
// ---------------------------------------------------------------------------
static field_elem_t mod_sub(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    if (a >= b) {
        return a - b;
    }
    return (a + FIELD_P) - b;
}

// ---------------------------------------------------------------------------
// Modular multiplication: (a * b) mod p
// Uses 512-bit intermediate product then reduction.
// ---------------------------------------------------------------------------
static field_elem_t mod_mul(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    ap_uint<2 * FIELD_BITS> prod = ap_uint<2 * FIELD_BITS>(a) * b;
    return field_elem_t(prod % FIELD_P_512);
}

// ---------------------------------------------------------------------------
// Shared affine line-evaluation rule (from the paper):
//   line_eval(f0, f1, z) = f0 * (1 - z) + f1 * z
//                         = f0 + (f1 - f0) * z
//
// Used for both MLE extension (z ∈ {0..d}) and update (z = verifier challenge r).
// ---------------------------------------------------------------------------
static field_elem_t affine_line_eval(field_elem_t f0, field_elem_t f1, field_elem_t z) {
#pragma HLS INLINE
    field_elem_t diff = mod_sub(f1, f0);
    return mod_add(f0, mod_mul(diff, z));
}

#endif // ZKPHIRE_FIELD_ARITHMETIC_HPP
