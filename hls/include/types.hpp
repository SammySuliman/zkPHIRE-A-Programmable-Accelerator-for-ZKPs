#ifndef ZKPHIRE_TYPES_HPP
#define ZKPHIRE_TYPES_HPP

#include <ap_int.h>

// ---------------------------------------------------------------------------
// BN254 scalar field (order of the BN254 curve group)
//
// p = 21888242871839275222246405745257275088548364400416034343698204186575808495617
//   = 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001
//
// Field elements fit in 254 bits; we use ap_uint<256> for headroom.
// Low-valued elements (0..MAX_DEGREE) are used as evaluation points.
// ---------------------------------------------------------------------------

static const int FIELD_BITS = 256;

using field_elem_t = ap_uint<FIELD_BITS>;

// BN254 scalar field prime
static const field_elem_t FIELD_P = field_elem_t(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

// ---------------------------------------------------------------------------
// Compile-time parameters — configurable for the one-round SumCheck core
// ---------------------------------------------------------------------------

// Maximum term degree (number of constituent MLE factors in one product term).
// The HLS core produces degree+1 round samples per invocation.
static const int MAX_DEGREE = 5;
static const int MAX_SAMPLES = MAX_DEGREE + 1;  // d+1 samples

// Maximum MLE table size (must be a power of two). The one-round core
// consumes a full table and produces a halved table.
static const int MAX_TABLE_SIZE = 256;

// Convenience for the AXI stream width
static const int DATA_WIDTH = FIELD_BITS;

// Error/status codes
using status_t = ap_uint<4>;
static const status_t STATUS_OK          = 0;
static const status_t STATUS_BAD_DEGREE  = 1;
static const status_t STATUS_BAD_SIZE    = 2;
static const status_t STATUS_BAD_CHALLENGE = 3;

#endif // ZKPHIRE_TYPES_HPP
