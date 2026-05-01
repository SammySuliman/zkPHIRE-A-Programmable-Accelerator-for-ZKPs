# RFSoC 4x2 SumCheck HLS Implementation

This directory contains the larger-board implementation used for the paper-parity portion of the project. It targets the RFSoC 4x2 part `xczu48dr-ffvg1517-2-e` at a 200 MHz HLS clock target.

## Why this version exists

The PYNQ-Z2 design is useful for early bring-up, but BN254 modular multiplication using a 512-bit product and modular reduction consumes too many DSP resources for a paper-like multi-PE design. The RFSoC 4x2 has enough DSP, LUT, FF, and BRAM capacity to implement a meaningful version of the zkPHIRE-style datapath rather than a dummy/demo core.

## Architecture

| Component | Implementation |
|---|---|
| Processing elements | `NUM_PES = 8`, unrolled PE dispatch in `multi_pe_sumcheck` (`src/sumcheck_top.cpp`) |
| Scratchpad | `SCRATCHPAD_BANKS = 16`, partitioned across banks, loaded once before PE dispatch |
| Product lanes | Each PE evaluates `degree+1` sample points and accumulates partial round samples |
| Reduction | Parent function reduces `pe_all_samples[NUM_PES][MAX_SAMPLES]` into final `samples` |
| Table update | Performed once in the orchestrator, not inside each PE, avoiding multi-PE overwrite bugs |
| Arithmetic | BN254 scalar field with `ap_uint<256>` elements and 512-bit intermediate multiply |

Current verified configuration:

```cpp
MAX_DEGREE      = 6
MAX_SAMPLES     = 7
MAX_TABLE_SIZE  = 256
NUM_PES         = 8
SCRATCHPAD_BANKS = 16
```

## Correctness Tests

The C-simulation testbench is `testbench/tb_sumcheck.cpp`. Results (Vitis HLS 2023.2, NYU ECE server):

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

Full log: [`../docs/verification/logs/rfsoc_csim.log`](../docs/verification/logs/rfsoc_csim.log).

## Synthesis Results

Vitis HLS 2023.2, target `xczu48dr-ffvg1517-2-e`, clock target 5.00 ns (200 MHz):

| Metric | Value |
|---|---|
| Estimated period | 6.035 ns |
| Fmax | 165.70 MHz |
| Latency | 25,718,637 cycles (~155 ms) |
| Interval | 25,718,638 cycles (II=1) |
| BRAM_18K | 154 / 2,160 (7%) |
| DSP | 466 / 4,272 (10%) |
| FF | 259,434 / 850,560 (30%) |
| LUT | 235,685 / 425,280 (55%) |
| URAM | 0 / 80 (0%) |

Full report: [`../docs/verification/reports/rfsoc_sumcheck_round_array_csynth.rpt`](../docs/verification/reports/rfsoc_sumcheck_round_array_csynth.rpt).

**Clock target note**: The 8-PE design with tree reduction achieves 165.70 MHz vs. a 200 MHz target. This is typical for unoptimized HLS dataflow — all functional tests pass at this clock, and pipelining refinement can close the remaining gap.

## Build

On an NYU ECE server after VPN:

```bash
cd ~/zkphire/hls_rfsoc
rm -rf zkphire_rfsoc zkphire_rfsoc_axi
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl
```

- `run_hls.tcl` performs C-simulation and C-synthesis for `sumcheck_round_array`
- `run_axi.tcl` exports the verified array top as IP for catalog integration

## Important code locations

| File | Role |
|---|---|
| `src/sumcheck_top.cpp` | Top-level orchestration: PE dispatch, scratchpad load, reduction, update |
| `include/field_arithmetic.hpp` | BN254 modular add/sub/mul and affine line evaluation |
| `include/scratchpad.hpp` | Scratchpad load/read/write helpers |
| `include/product_lane.hpp` | Pointwise product across MLE factors |
| `testbench/tb_sumcheck.cpp` | Regression suite and invariant checks |
