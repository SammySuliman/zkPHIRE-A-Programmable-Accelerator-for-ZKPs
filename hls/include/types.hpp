#ifndef ZKPHIRE_TYPES_HPP
#define ZKPHIRE_TYPES_HPP

#include <ap_int.h>

// ---------------------------------------------------------------------------
// BN254 scalar field (order of the BN254 curve group)
// ---------------------------------------------------------------------------
// p = 21888242871839275222246405745257275088548364400416034343698204186575808495617
// p fits in 254 bits; we use ap_uint<256> for headroom.
// ---------------------------------------------------------------------------

static const int FIELD_BITS = 256;

using field_elem_t = ap_uint<FIELD_BITS>;

// BN254 scalar field prime (254-bit)
static const field_elem_t FIELD_P = field_elem_t(
    "21888242871839275222246405745257275088548364400416034343698204186575808495617"
);

// ---------------------------------------------------------------------------
// Compile-time parameters — configurable for the one-round SumCheck core
// ---------------------------------------------------------------------------

// Maximum term degree (number of constituent MLE factors in one product term).
// The HLS core produces degree+1 round samples.
static const int MAX_DEGREE = 6;

// Maximum MLE table size (must be a power of two). The one-round core
// consumes a full table and produces a halved table.
static const int MAX_TABLE_SIZE = 256;

// ---------------------------------------------------------------------------
// Field arithmetic primitives (bit-exact with the Python golden model)
// ---------------------------------------------------------------------------

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
    // Compute (a - b) mod p safely without underflow
    if (a >= b) {
        return a - b;
    }
    return (a + FIELD_P) - b;
}

static field_elem_t mod_mul(field_elem_t a, field_elem_t b) {
#pragma HLS INLINE
    // BN254 field prime is ~254 bits; product fits in 512 bits.
    ap_uint<2 * FIELD_BITS> prod = ap_uint<2 * FIELD_BITS>(a) * b;
    return field_elem_t(prod % FIELD_P);
}

// ---------------------------------------------------------------------------
// Shared affine line-evaluation rule (from the paper; used for both
// extension and update):
//   line_eval(f0, f1, z) = f0 * (1 - z) + f1 * z
//                         = f0 + (f1 - f0) * z
// ---------------------------------------------------------------------------

static field_elem_t affine_line_eval(field_elem_t f0, field_elem_t f1, field_elem_t z) {
#pragma HLS INLINE
    field_elem_t diff = mod_sub(f1, f0);
    return mod_add(f0, mod_mul(diff, z));
}

#endif // ZKPHIRE_TYPES_HPP
