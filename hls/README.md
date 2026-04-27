# zkPHIRE SumCheck — Vitis HLS Implementation

Hardware accelerator for one programmable SumCheck round, targeting Xilinx Zynq PYNQ-Z2 (`xc7z020clg400-1`).

**Status:** Phase 0 (Golden Model) ✓ | Phase 1 (HLS Core) ✓ | Phase 2 (Multi-Term) ✓

## Architecture

```
hls/
├── include/
│   ├── types.hpp            # BN254 field arithmetic (ap_uint<256>)
│   ├── update_unit.hpp      # Affine MLE update at verifier challenge r
│   ├── extension_engine.hpp # Extend MLE pair → d+1 point evaluations
│   ├── product_lane.hpp     # Per-point product across MLE factors
│   └── accumulator.hpp      # Round-sample accumulator (d+1 registers)
├── src/
│   └── sumcheck_top.cpp     # Top-level one-round orchestration
├── testbench/
│   └── tb_sumcheck.cpp      # C-simulation test harness
├── data/                    # Generated test vectors (from vecgen.py)
├── run_hls.tcl              # Vitis HLS build script
├── vecgen.py                # Test vector generator (from golden model)
├── phase2_acc.py            # Phase 2 multi-term accumulation verification
└── README.md
```

## Dataflow (One Round)

For one product term of degree `d`, per the zkPHIRE paper:

1. **Load** MLE tables `tables[mle_idx][0..size-1]` into BRAM
2. **Pair** read `(tables[mle_idx][2k], tables[mle_idx][2k+1])` for each `k`
3. **Extend** each pair to `d+1` evaluations via affine rule: `f(z) = f0 + (f1-f0)*z`
4. **Multiply** per-factor extensions pointwise → `d+1` lane products
5. **Accumulate** lane products over all `size/2` pairs → `d+1` round samples
6. **Update** each MLE table with verifier challenge `r` → halved tables

The shared affine rule is used for both extension (step 3) and update (step 6).

## Quick Start

### 1. Verify golden model (Phase 0)
```bash
python3 ../golden_sumcheck.py --test
```

### 2. Generate test vectors
```bash
python3 vecgen.py --all
```

### 3. Run Vitis HLS C-simulation
```bash
# On the ECE server (ecs02-06.poly.edu) after connecting via NYU VPN:
cd fp_zkphire/hls
vitis_hls -f run_hls.tcl
```

### 4. Verify multi-term accumulation (Phase 2)
```bash
python3 phase2_acc.py
```

## Deterministic Test Targets

Per SPEC Section 8:

| Case | Expression | Challenges | Round 1 samples | Final |
|------|-----------|------------|-----------------|-------|
| A | x1·x2·x3 | (5, 7, 11) | [0, 1, 2, 3] | 385 |
| B | x1·x2 + x2·x3 | (5, 7, 11) | [1, 3, 5] | 112 |
| C | deg-4 Fig.1 term | r=5 | — | — |

## Field Arithmetic (BN254)

- Prime: `21888242871839275222246405745257275088548364400416034343698204186575808495617`
- Element width: 256 bits (`ap_uint<256>`)
- Multiplication: 512-bit intermediate → modulo reduction
- All operations are bit-exact with Python golden model

## Development Phases

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Golden model + deterministic regressions | ✓ Complete |
| 1 | Functional HLS one-round core | ✓ Complete |
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
