#ifndef ZKPHIRE_FIELD_ARITHMETIC_HPP
#define ZKPHIRE_FIELD_ARITHMETIC_HPP

#include "types.hpp"

// ---------------------------------------------------------------------------
// BN254 scalar field arithmetic
//
// DSP optimization strategy:
//   mod_mul uses ap_uint % but with BIND_OP to force a radix-2 divider
//   (fewer DSPs than default radix-4) plus ALLOCATION to share instances.
//
//   The modulo operator on 512-bit numbers is the bottleneck.
//   Using BIND_OP op=urem impl=dsp reduces to a fixed-function divider
//   rather than synthesizing one from scratch each time.
// ---------------------------------------------------------------------------

static const ap_uint<512> FIELD_P_512 = ap_uint<512>(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

static field_elem_t mod_mul(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE off
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
