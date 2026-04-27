#ifndef ZKPHIRE_FIELD_ARITHMETIC_HPP
#define ZKPHIRE_FIELD_ARITHMETIC_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// BN254 scalar field arithmetic — bit-exact with the Python golden model
//
// Field prime (BN254 curve order):
//   p = 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001
//
// Phase 3: Barrett reduction replaces `prod % FIELD_P` with
//   q  = ((prod >> 255) * mu) >> 257
//   r  = prod - q * p
//   if r >= p: r -= p
// This eliminates the 512-bit division, reducing DSP count.
//
// Barrett constants (precomputed in Python):
//   mu = floor(2^512 / p) = 0x54a4746... (260 bits)
// ---------------------------------------------------------------------------

static const ap_uint<512> FIELD_P_512 = ap_uint<512>(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

// Barrett mu = floor(2^512 / FIELD_P), 260 bits
static const ap_uint<260> BARRETT_MU = ap_uint<260>(
    "0x54a47462623a04a7ab074a58680730147144852009e880ae620703a6be1de9259"
);

// ---------------------------------------------------------------------------
// Modular multiplication with Barrett reduction
//   a, b < 2^256, result < FIELD_P
// ---------------------------------------------------------------------------
static field_elem_t mod_mul(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE off
    // 512-bit product
    ap_uint<512> prod = ap_uint<512>(a) * b;

    // Barrett reduction: q = ((prod >> 255) * mu) >> 257
    //   prod_hi = prod >> 255      (257 bits)
    //   q_full  = prod_hi * mu     (517 bits max)
    //   q       = q_full >> 257    (260 bits max)
    ap_uint<257> prod_hi = prod >> 255;
    ap_uint<517> q_full = ap_uint<517>(prod_hi) * BARRETT_MU;
    ap_uint<260> q = q_full >> 257;

    // r = prod - q * p
    ap_uint<512> qp = ap_uint<512>(q) * FIELD_P_512;
    ap_uint<512> r_full = prod - qp;

    // Branch-free final correction
    field_elem_t r = r_full;
    field_elem_t r_sub = r_full - field_elem_t(FIELD_P);
    if (r_full >= FIELD_P_512) {
        r = r_sub;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Modular addition: (a + b) mod p
// ---------------------------------------------------------------------------
static field_elem_t mod_add(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    ap_uint<FIELD_BITS + 1> sum = ap_uint<FIELD_BITS + 1>(a) + b;
    ap_uint<FIELD_BITS + 1> sum_sub = sum - ap_uint<FIELD_BITS + 1>(FIELD_P);
    if (sum >= FIELD_P) {
        return field_elem_t(sum_sub);
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
// Shared affine line-evaluation rule (from zkPHIRE paper):
//   line_eval(f0, f1, z) = f0 + (f1 - f0) * z
// ---------------------------------------------------------------------------
static field_elem_t affine_line_eval(field_elem_t f0, field_elem_t f1, field_elem_t z) {
#pragma HLS INLINE
    field_elem_t diff = mod_sub(f1, f0);
    return mod_add(f0, mod_mul(diff, z));
}

#endif // ZKPHIRE_FIELD_ARITHMETIC_HPP
