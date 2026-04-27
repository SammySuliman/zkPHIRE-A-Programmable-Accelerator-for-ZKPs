# zkPHIRE SumCheck — Vitis HLS Implementation

Hardware accelerator for one programmable SumCheck round, targeting Xilinx Zynq PYNQ-Z2 (`xc7z020clg400-1`).

**Status:** Phase 0 (Golden Model) ✓ | Phase 1 (HLS Core) ✓ | Phase 2 (Multi-Term) ✓

## Architecture

```
hls/
├── include/
│   ├── types.hpp               # BN254 types, constants, status codes
│   ├── field_arithmetic.hpp    # mod_add, mod_sub, mod_mul, affine_line_eval
│   ├── update_unit.hpp         # Affine MLE update at verifier challenge r
│   ├── extension_engine.hpp    # Extend MLE pair → d+1 point evaluations
│   ├── product_lane.hpp        # Per-point product across MLE factors
│   └── accumulator.hpp         # Round-sample accumulator (d+1 registers)
├── src/
│   └── sumcheck_top.cpp        # Two APIs: sumcheck_round_array + sumcheck_round_axi
├── testbench/
│   └── tb_sumcheck.cpp         # C-simulation test harness (10 test cases)
├── run_hls.tcl                 # Vitis HLS build script
├── vecgen.py                   # Test vector generator (from golden model)
├── phase2_acc.py               # Phase 2 multi-term accumulation verification
└── README.md
```

## Two APIs

### `sumcheck_round_array` — C-Sim / BRAM

Fast verification interface. Uses partitioned BRAM arrays, ideal for initial testing.

```cpp
status_t sumcheck_round_array(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],  // input MLE tables
    field_elem_t r,          // verifier challenge
    int degree,              // number of MLE factors
    int size,                // table size (power of two)
    field_elem_t samples[MAX_SAMPLES],                      // output: s(0..d)
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]    // output: halved tables
);
```

Returns `STATUS_OK` (0) or an error code.

### `sumcheck_round_axi` — m_axi / Board-Ready

DMA-friendly interface. MLE tables read from DRAM, results written back to DRAM.

```cpp
status_t sumcheck_round_axi(
    field_elem_t* mle_inputs,     // gmem0: input tables [degree][size]
    int degree,
    int size,
    field_elem_t r,
    field_elem_t* round_samples,  // gmem1: output s(0..d)
    field_elem_t* next_tables     // gmem2: output updated tables
);
```

## Dataflow (One Round)

For one product term of degree `d`, per the zkPHIRE paper:

1. **Load** MLE tables into local BRAM
2. **Pair** read `(tables[mle_idx][2k], tables[mle_idx][2k+1])` for each `k`
3. **Extend** each pair to `d+1` evaluations via affine rule: `f(z) = f0 + (f1-f0)*z`
4. **Multiply** per-factor extensions pointwise → `d+1` lane products
5. **Accumulate** lane products over all `size/2` pairs → `d+1` round samples
6. **Update** each MLE table with verifier challenge `r` → halved tables

## Quick Start

### 1. Verify golden model (Phase 0)
```bash
python3 golden_sumcheck.py --test
```

### 2. Generate test vectors
```bash
cd hls && python3 vecgen.py --all
```

### 3. Run Vitis HLS C-simulation
```bash
# On the ECE server (ecs02-06.poly.edu) after NYU VPN:
cd hls && vitis_hls -f run_hls.tcl
```

### 4. Verify multi-term accumulation (Phase 2)
```bash
python3 hls/phase2_acc.py
```

## Deterministic Test Targets

Per SPEC Section 8:

| Case | Expression | Challenges | Round 1 samples | Final |
|------|-----------|------------|-----------------|-------|
| A | x1·x2·x3 | (5, 7, 11) | [0, 1, 2, 3] | 385 |
| B | x1·x2 + x2·x3 | (5, 7, 11) | [1, 3, 5] | 112 |
| C | deg-4 Fig.1 term | r=5 | — | — |

## Field Arithmetic (BN254 Scalar Field)

- Prime: `0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001`
- Element width: 256 bits (`ap_uint<256>`)
- Multiplication: 512-bit intermediate → modulo reduction
- All operations are bit-exact with Python golden model

## Development Phases

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Golden model + deterministic regressions | ✓ Complete |
| 1 | Functional HLS one-round core (array + AXI APIs) | ✓ Complete |
| 2 | Multi-term accumulation | ✓ Complete |
| 3 | Paper-aligned optimizations (pipelining, lanes) | Pending |
| 4 | PYNQ/board integration (AXI, DMA) | Pending |

## Synthesis Targets

- **Board:** PYNQ-Z2 (`xc7z020clg400-1`)
- **Clock:** 100 MHz (10 ns period)
- **Degree:** up to 6 (first milestone)
- **DSP budget:** < 180
- **BRAM budget:** < 80%

## Invariants Verified

1. `s(0) + s(1)` = claimed sum over full table
2. Updated table claim = `s(r)`
3. Table size halved exactly
4. `r=0` selects even entries, `r=1` selects odd entries
5. Full protocol chain collapses to correct final scalar
