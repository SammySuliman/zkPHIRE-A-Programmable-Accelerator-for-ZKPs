# RFSoC 4x2 SumCheck HLS Implementation

This directory contains the larger-board implementation used for the paper-parity portion of the project. It targets the RFSoC 4x2 part `xczu48dr-ffvg1517-2-e` at a 200 MHz HLS clock target.

## Why this version exists

The PYNQ-Z2 design is useful for early bring-up, but BN254 modular multiplication using a 512-bit product and modular reduction consumes too many DSP resources for a paper-like multi-PE design. The RFSoC 4x2 has enough DSP, LUT, FF, and BRAM capacity to implement a meaningful version of the zkPHIRE-style datapath rather than a dummy/demo core.

## Architecture

| Component | Implementation |
|---|---|
| Processing elements | `NUM_PES = 8`, unrolled PE dispatch in `multi_pe_sumcheck` (`src/sumcheck_top.cpp`). |
| Scratchpad | `SCRATCHPAD_BANKS = 16`, partitioned across banks, loaded once before PE dispatch. |
| Product lanes | Each PE evaluates `degree+1` sample points and accumulates partial round samples. |
| Reduction | Parent function reduces `pe_all_samples[NUM_PES][MAX_SAMPLES]` into final `samples`. |
| Table update | Performed once in the orchestrator, not inside each PE, avoiding multi-PE overwrite bugs. |
| Arithmetic | BN254 scalar field with `ap_uint<256>` elements and 512-bit intermediate multiply. |

Current verified configuration:

```cpp
MAX_DEGREE      = 6
MAX_SAMPLES     = 7
MAX_TABLE_SIZE  = 256
NUM_PES         = 8
SCRATCHPAD_BANKS = 16
```

## Correctness Tests

The C-simulation testbench is `testbench/tb_sumcheck.cpp`. It checks:

- Case A: `x1*x2*x3`, including `r=0`, `r=1`, and challenge-chain final value `385`.
- Case B: `x1*x2`, `x2*x3`, and combined term `x1*x2 + x2*x3 -> [1,3,5]`.
- Structural invariants for sample polynomial and table update.

Latest log excerpts and synthesis tables are in `../docs/verification/`.

## Build

On an NYU ECE server after VPN:

```bash
cd ~/zkphire/hls_rfsoc
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
```

`run_hls.tcl` performs C-simulation and C-synthesis for `sumcheck_round_array`.

`run_axi.tcl` exports the verified array top as IP for catalog integration. The AXI-Stream interface exploration was deferred; the board-facing DMA message contract remains documented for the PYNQ-Z2 `hls/sumcheck_round_axi` implementation.

## Important code locations

| File | Role |
|---|---|
| `src/sumcheck_top.cpp` | Top-level orchestration: PE dispatch, scratchpad load, reduction, update. |
| `include/field_arithmetic.hpp` | BN254 modular add/sub/mul and affine line evaluation. |
| `include/scratchpad.hpp` | Scratchpad load/read/write helpers. |
| `include/product_lane.hpp` | Pointwise product across MLE factors. |
| `testbench/tb_sumcheck.cpp` | Regression suite and invariant checks. |
