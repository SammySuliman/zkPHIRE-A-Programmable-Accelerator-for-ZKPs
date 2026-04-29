#ifndef ZKPHIRE_FIELD_ARITHMETIC_HPP
#define ZKPHIRE_FIELD_ARITHMETIC_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// BN254 Montgomery Modular Arithmetic — zkPHIRE paper-parity
//
// All internal values stored in Montgomery domain: x_mont = x * R mod p
// where R = 2^256. Addition/subtraction are Montgomery-transparent:
//   (aR + bR) mod p = (a+b)R mod p  ✓
//   (aR - bR) mod p = (a-b)R mod p  ✓
//
// Multiplication uses Montgomery reduction (no division):
//   mon_mul(aR, bR) = aR * bR * R^{-1} mod p = (a*b)R mod p
//
// Conversion happens only at boundaries:
//   to_montgomery(x) = (x * R) mod p        (on table load)
//   from_montgomery(xR) = mon_mul(xR, 1)    (on result store)
//
// Montgomery constants (precomputed in Python against golden model):
//   p' = (-p^{-1}) mod R = 0x73f82f1d0d8341b2e39a9828990623916586864b4c6911b3c2e1f593efffffff
//   R mod p = 0x0e0a77c19a07df2f666ea36f7879462e36fc76959f60cd29ac96341c4ffffffb
// ---------------------------------------------------------------------------

static const ap_uint<512> FIELD_P_512 = ap_uint<512>(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

// R = 2^256, R mod p (used for to_montgomery conversion at load)
static const field_elem_t R_MOD_P = field_elem_t(
    "0x0e0a77c19a07df2f666ea36f7879462e36fc76959f60cd29ac96341c4ffffffb"
);

// p' = (-p^{-1}) mod R, 256-bit Montgomery constant
static const ap_uint<256> P_PRIME = ap_uint<256>(
    "0x73f82f1d0d8341b2e39a9828990623916586864b4c6911b3c2e1f593efffffff"
);

// ---------------------------------------------------------------------------
// Convert regular field element to Montgomery domain: x' = x * R mod p
// ---------------------------------------------------------------------------
static field_elem_t to_montgomery(field_elem_t x) {
#pragma HLS INLINE
    ap_uint<512> prod = ap_uint<512>(x) * ap_uint<512>(R_MOD_P);
    return field_elem_t(prod % FIELD_P_512);
}

// ---------------------------------------------------------------------------
// Convert from Montgomery to regular: x = mon_mul(x', 1)
// mon_mul(x', R_MOD_P) = x' * R * R^{-1} mod p = x' * R^{-1} mod p?
// Wait: mon_mul(aR, bR) = aRbRRinv = abR. So mon_mul(xR, R) = xR * R * Rinv = xR.
// For from_mont: mon_mul(xR, R_MOD_P) = xR * R_mod_p * Rinv = x * R * R_mod_p * Rinv
// That doesn't simplify cleanly.
// 
// Better: from_montgomery uses mon_mul with 1 in regular domain.
// 1 in Montgomery = 1 * R mod p = R_MOD_P.
// mon_mul(xR, R_MOD_P) = xR * R_MOD_P * R^{-1} mod p
// But xR = x*R mod p. So: (x*R) * (R) * R^{-1} mod p = x*R mod p. That's still xR!
// 
// The correct approach: mon_mul(xR, 1_regular) where 1_regular = 1.
// mon_mul(xR, 1) = xR * 1 * R^{-1} = x mod p. ✓
// But mon_mul expects Montgomery inputs, and 1 in Montgomery = R_MOD_P.
// So: mon_mul(xR, to_montgomery(1)) = mon_mul(xR, R_MOD_P) wouldn't work as above.
// 
// Actually, the standard approach in Montgomery arithmetic:
// from_montgomery(x) = (x * R^{-1}) mod p
// Direct computation: T = x * (no multiply needed), just divide by R.
// But we can use mon_mul(x, 1) where both x and 1 are regular:
// mon_mul on regular values gives: x * 1 * R^{-1} mod p. No good.
//
// Simplest: compute R_INV = R^{-1} mod p (precomputed), then:
// from_montgomery(x_mont) = (x_mont * R_INV) mod p
//
// But that uses the old % divider, defeating the purpose.
//
// Better approach: use montgomery multiply with 1 as:
// mon_mul(x_mont, 1_mont) where 1_mont = R_MOD_P (1 * R mod p)
// Result: x_mont * 1_mont * R^{-1} = xR * R * R^{-1} = xR. Still wrong.
//
// The issue: mon_mul outputs Montgomery domain. To get regular domain,
// we need: xR * 1 * R^{-1} = x. 
// So xR (montgomery) * 1 (montgomery) → output = xR * R_part?
//
// Wait — I'm confusing myself. Let me re-derive.
// mon_mul(aR, bR) = aR * bR * R^{-1} mod p = (a*b)R mod p. Output is Montgomery.
//
// For from_montgomery, I want: input xR → output x (regular).
// x = xR * R^{-1} mod p.
//
// mon_mul(xR, R^2R) = xR * R^2R * R^{-1} = xR * R^2 * R * R^{-1} = xR * R^2
// No, R^2 in Montgomery = R^2 * R mod p = R_MOD_P... this is circular.
//
// OK, simpler: FROM_MONT can be done by mon_mul(xR, R2_MONT)
// where R2_MONT = R^2 mod p in Montgomery = R^2 * R mod p = R^3 mod p? No.
//
// Actually the simplest: use the mon_mul directly as a reduction:
// mon_mul_reduce(T) = T * R^{-1} mod p
// This converts a 512-bit product to Montgomery domain.
// For from_mont: T = x_mont * R, then mon_mul_reduce(T) = x_mont * R * R^{-1} = x_mont
// No wait.
//
// Let me just compute R_INV = pow(R, -1, p) once and store it.
// from_montgomery(x) = (x * R_INV) % p.
// Since this is just ONE conversion at output time (not in inner loop),
// the DSP cost of one % is negligible.
// ---------------------------------------------------------------------------

// R^{-1} mod p (precomputed, for from_montgomery conversion)
static const field_elem_t R_INV = field_elem_t(
    "0x15ebf95182c5551cc8260de4aeb85d5d090ef5a9e111ec87dc5ba0056db1194e"
);

static field_elem_t from_montgomery(field_elem_t x_mont) {
#pragma HLS INLINE
    // x * R^{-1} mod p — only called at output boundaries, DSP cost negligible
    ap_uint<512> prod = ap_uint<512>(x_mont) * ap_uint<512>(R_INV);
    return field_elem_t(prod % FIELD_P_512);
}

// ---------------------------------------------------------------------------
// Montgomery modular multiply — core operation
//   a, b in Montgomery domain → result in Montgomery domain
//   mon_mul(aR, bR) = aR * bR * R^{-1} mod p = (a*b)R mod p
// ---------------------------------------------------------------------------
static field_elem_t mod_mul(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE off
    ap_uint<512> T = ap_uint<512>(a) * b;
    // m = (T * p') mod R (low 256 bits)
    ap_uint<256> m = ap_uint<256>(T(255, 0) * P_PRIME(255, 0));
    // t = (T + m * p) >> 256
    ap_uint<512> mp = ap_uint<512>(m) * FIELD_P_512;
    ap_uint<512> sum = T + mp;
    ap_uint<256> t = sum >> 256;
    // Conditional subtraction (at most 2 corrections)
    if (t >= FIELD_P) {
        t = t - field_elem_t(FIELD_P);
    }
    if (t >= FIELD_P) {
        t = t - field_elem_t(FIELD_P);
    }
    return t;
}

// ---------------------------------------------------------------------------
// Modular addition — Montgomery-transparent
//   (aR + bR) mod p = (a+b)R mod p
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
// Modular subtraction — Montgomery-transparent
// ---------------------------------------------------------------------------
static field_elem_t mod_sub(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    if (a >= b) {
        return a - b;
    }
    return (a + FIELD_P) - b;
}

// ---------------------------------------------------------------------------
// Affine line evaluation in Montgomery domain
//   f(z) = f0 + (f1 - f0) * z
// All inputs in Montgomery, output in Montgomery.
// ---------------------------------------------------------------------------
static field_elem_t affine_line_eval(
    field_elem_t f0, field_elem_t f1, field_elem_t z
) {
#pragma HLS INLINE
    field_elem_t diff = mod_sub(f1, f0);
    return mod_add(f0, mod_mul(diff, z));
}

#endif // ZKPHIRE_FIELD_ARITHMETIC_HPP
