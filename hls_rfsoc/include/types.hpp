#ifndef ZKPHIRE_TYPES_HPP
#define ZKPHIRE_TYPES_HPP

#include <ap_int.h>

// ---------------------------------------------------------------------------
// zkPHIRE SumCheck Accelerator — RFSoC 4x2 (xczu48dr-ffvg1517-2-e)
//
// Paper-parity-scaled implementation: 8 PEs and 16 scratchpad banks.
// Current verified configuration supports degree ≤ 6 and table size ≤ 256.
// Arithmetic: regular-domain BN254 with shared modular multiplier.
// ---------------------------------------------------------------------------

static const int FIELD_BITS = 256;
using field_elem_t = ap_uint<FIELD_BITS>;

static const field_elem_t FIELD_P = field_elem_t(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

static const int MAX_DEGREE = 6;
static const int MAX_SAMPLES = MAX_DEGREE + 1;
static const int MAX_TABLE_SIZE = 256;

static const int NUM_PES = 8;
static const int SCRATCHPAD_BANKS = 16;
static const int SCRATCHPAD_DEPTH = MAX_TABLE_SIZE / 2;

using status_t = ap_uint<4>;
static const status_t STATUS_OK           = 0;
static const status_t STATUS_BAD_DEGREE   = 1;
static const status_t STATUS_BAD_SIZE     = 2;
static const status_t STATUS_BAD_CHALLENGE = 3;

#endif // ZKPHIRE_TYPES_HPP
