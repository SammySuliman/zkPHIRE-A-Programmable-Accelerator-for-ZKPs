#ifndef ZKPHIRE_TYPES_HPP
#define ZKPHIRE_TYPES_HPP

#include <ap_int.h>
#include <hls_stream.h>

// ---------------------------------------------------------------------------
// zkPHIRE SumCheck Accelerator — RFSoC 4x2 (xczu48dr-ffvg1517-2-e)
//
// Full paper-parity: Montgomery arithmetic, 8 PEs, 16 scratchpad banks,
// degree ≤ 16, AXI-Stream DMA interface.
// ---------------------------------------------------------------------------

static const int FIELD_BITS = 256;
using field_elem_t = ap_uint<FIELD_BITS>;

// BN254 scalar field prime
static const field_elem_t FIELD_P = field_elem_t(
    "0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001"
);

// Paper-parity parameters (matching zkPHIRE Figure 3-4)
static const int MAX_DEGREE = 16;
static const int MAX_SAMPLES = MAX_DEGREE + 1;
static const int MAX_TABLE_SIZE = 1024;

// PE and scratchpad architecture
static const int NUM_PES = 8;
static const int SCRATCHPAD_BANKS = 16;
static const int SCRATCHPAD_DEPTH = MAX_TABLE_SIZE / 2;

// Status codes
using status_t = ap_uint<4>;
static const status_t STATUS_OK           = 0;
static const status_t STATUS_BAD_DEGREE   = 1;
static const status_t STATUS_BAD_SIZE     = 2;
static const status_t STATUS_BAD_CHALLENGE = 3;

// AXI stream width
static const int DATA_WIDTH = FIELD_BITS;
using axis_word_t = hls::axis<ap_uint<DATA_WIDTH>, 0, 0, 0>;

#endif // ZKPHIRE_TYPES_HPP
