# zkPHIRE SumCheck — PYNQ-Z2 Vitis HLS Implementation

This directory contains the PYNQ-Z2 implementation of one programmable SumCheck round over the BN254 scalar field. It is the smallest board-facing version of the design and includes both a BRAM C-simulation interface and an `m_axi` DMA-style interface.

## Architecture

```
hls/
├── include/
│   ├── types.hpp            # field type, constants, status codes
│   ├── field_arithmetic.hpp # BN254 add/sub/mul and affine line evaluation
│   ├── update_unit.hpp      # verifier-challenge table update
│   ├── extension_engine.hpp # pair extension to d+1 sample points
│   ├── product_lane.hpp     # pointwise product across MLE factors
│   ├── accumulator.hpp      # modular accumulation of round samples
│   └── scratchpad.hpp       # banked scratchpad helper routines
├── src/sumcheck_top.cpp     # top functions: BRAM array and m_axi DMA
├── testbench/tb_sumcheck.cpp
├── run_hls.tcl              # C-sim + synthesis for sumcheck_round_array
└── run_axi.tcl              # synthesis/IP export for sumcheck_round_axi
```

## One-Round Dataflow

For one product term of degree `d`:

1. Read pair `(tables[m][2k], tables[m][2k+1])` from each MLE factor `m`.
2. Extend each pair to sample points `z=0..d` using `f(z)=f0+(f1-f0)z`.
3. Multiply the `d` extensions pointwise to form lane products.
4. Accumulate lane products over all pairs to produce `samples[0..d]`.
5. Update each MLE table at verifier challenge `r` to produce `updated[m][k]`.

## Concrete Interfaces

### BRAM Array Top

```cpp
status_t sumcheck_round_array(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],
    field_elem_t r,
    int degree,
    int size,
    field_elem_t samples[MAX_SAMPLES],
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]
);
```

- `tables[m][i]`: input MLE table row `m`, element `i`.
- `samples[x]`: round polynomial sample at `x`, for `x=0..degree`.
- `updated[m][k]`: next-round table row `m`, element `k`, with `size/2` active entries.
- Scalar control registers: `degree`, `size`, `r`, and status return.

### m_axi DMA Top

```cpp
status_t sumcheck_round_axi(
    field_elem_t* mle_inputs,
    int degree,
    int size,
    field_elem_t r,
    field_elem_t* round_samples,
    field_elem_t* next_tables
);
```

Flat memory layout:

| Buffer | Direction | Layout |
|---|---|---|
| `mle_inputs` | host -> IP | `mle_inputs[m*size + i] = tables[m][i]` |
| `round_samples` | IP -> host | `round_samples[x] = samples[x]`, `x=0..degree` |
| `next_tables` | IP -> host | `next_tables[m*(size/2) + k] = updated[m][k]` |

See [`../docs/interface-contract.md`](../docs/interface-contract.md) for status codes and host driving sequence.

## Verification

`testbench/tb_sumcheck.cpp` is self-checking and covers deterministic SPEC targets:

| Case | Expression | Expected |
|---|---|---|
| A | `x1*x2*x3` | first-round samples `[0,1,2,3]`, chain `(5,7,11) -> 385` |
| A edge | `x1*x2*x3` | `r=0` selects even entries, `r=1` selects odd entries |
| B | `x1*x2`, `x2*x3` | individual term samples pass |
| B combined | `x1*x2 + x2*x3` | combined samples `[1,3,5]` |

### C-Simulation Output

Vitis HLS 2023.2 C-simulation on NYU ECE server — 7/7 tests pass:

```
=== zkPHIRE SumCheck C-Simulation ===

PASS Case-A r=5
PASS Case-A r=0
PASS Case-A r=1
PASS Case-A chain
PASS Case-B x1*x2
PASS Case-B x2*x3
Case-B combined PASS

=== Results: 7/7 tests passed ===
```

Full log: [`../docs/verification/logs/pynq_csim.log`](../docs/verification/logs/pynq_csim.log).

### Synthesis Results

Vitis HLS 2023.2, target `xc7z020clg400-1`, clock 10.00 ns (100 MHz):

| Top Function | Est. Period | Fmax | BRAM_18K | DSP | FF | LUT |
|---|---|---|---|---|---|---|
| `sumcheck_round_array` (BRAM) | 7.145 ns | 139.96 MHz | 60/280 (21%) | 233/220 (105%) | 66,984/106,400 (62%) | 21,221/53,200 (39%) |
| `sumcheck_round_axi` (m_axi) | 7.396 ns | 135.21 MHz | 240/280 (85%) | 240/220 (109%) | 74,989/106,400 (70%) | 26,752/53,200 (50%) |

Full reports: [`../docs/verification/reports/`](../docs/verification/reports/).

### Efficiency Note

The design uses a **shared modular multiplier** (`#pragma HLS INLINE off` on `mod_mul`) to prevent Vitis from replicating the 512-bit multiplier at every call site. This reduces DSP from 458 → 233 (48% reduction) compared to per-call replication. Loops containing `mod_mul` use `#pragma HLS PIPELINE II=1` instead of `UNROLL` to reuse the shared multiplier.

## Build Commands

On the NYU ECE server after VPN:

```bash
cd ~/zkphire/hls
rm -rf zkphire_sumcheck zkphire_axi
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl
```

## Known Resource Limitation

The PYNQ-Z2 has 220 DSPs. Direct BN254 modular multiplication (`ap_uint<512> % FIELD_P`) is functionally correct but over the DSP budget (233/220). The shared-multiplier optimization brings it close; Montgomery multiplication (documented in [`../docs/paper-parity-plan.md`](../docs/paper-parity-plan.md)) would reduce DSP to ~60. The larger RFSoC version in `../hls_rfsoc/` demonstrates the multi-PE architecture within a realistic resource budget of 4,272 DSPs.
