# zkPHIRE SumCheck Accelerator

This repository implements and verifies a programmable FPGA accelerator for one SumCheck round over the BN254 scalar field. The design is based on the zkPHIRE paper's SumCheck datapath: table-pair reads, affine extension, product-lane multiplication, accumulation into a univariate round polynomial, and challenge-based table update for the next round.

**Team:** Sammy Suliman, Jui-Teng Huang, Ashesh Kaji  
**Branch:** `ashesh`  
**Paper reference:** [zkPHIRE: A Programmable Accelerator for ZKPs over High-Degree, Expressive Gates](https://arxiv.org/pdf/2508.16738)

## Repository Map

| Path | Purpose |
|---|---|
| `golden_sumcheck.py` | Python golden model and deterministic regression tests. |
| `hls/` | PYNQ-Z2 implementation: functional one-round core, BRAM API, m_axi API, C-sim testbench, Vitis TCL scripts. |
| `hls_rfsoc/` | Larger-board RFSoC 4x2 implementation with 8 processing elements and 16 scratchpad banks. |
| `docs/interface-contract.md` | Concrete host/IP message contract: scalar registers, BRAM memory layout, m_axi memory layout, status codes, host driving sequence. |
| `docs/verification/` | Submission evidence: live C-sim log excerpts, latency/throughput/resource tables, and exact ECE commands used. |
| `docs/paper/sumcheck-accelerator.pdf` | Short report with native TikZ architecture figure and measured synthesis results. |
| `docs/web/index.html` | Static interactive project explanation. |

## Implemented Operation

For a product term of degree `d` represented by `d` multilinear-extension (MLE) tables of length `size`, one hardware invocation computes:

1. For each pair index `k`, read `(f_i[2k], f_i[2k+1])` from every MLE table `i`.
2. Extend each pair to all round sample points `z = 0..d` with
   `L_i,k(z) = f_i[2k] + (f_i[2k+1] - f_i[2k]) z`.
3. Multiply across MLE factors pointwise: `P_k(z) = Π_i L_i,k(z)`.
4. Accumulate over pairs: `s(z) = Σ_k P_k(z)` for `z = 0..d`.
5. Update each MLE table for verifier challenge `r`: `f'_i[k] = L_i,k(r)`.

The returned `samples[0..d]` are evaluations of the univariate SumCheck round polynomial. `updated` is the halved table set used by the next round.

## Interfaces and Message Semantics

The project has two practical interfaces, both documented precisely in [`docs/interface-contract.md`](docs/interface-contract.md):

| Interface | Top function | Data movement | Intended use |
|---|---|---|---|
| BRAM array | `sumcheck_round_array` | Caller provides fixed HLS arrays: `tables`, `samples`, `updated`. | C-simulation, validation, block-design BRAM integration. |
| m_axi DMA | `sumcheck_round_axi` in `hls/` | Flat host buffers: `mle_inputs`, `round_samples`, `next_tables`, plus AXI-Lite config. | Board/DMA integration path. |

Both interfaces use AXI-Lite scalar control for `degree`, `size`, `r`, and status return. Status codes are `0=OK`, `1=bad degree`, `2=bad table size`, `3=bad challenge`.

## Verification Summary

The latest evidence is stored under [`docs/verification/`](docs/verification/). The current regression suite checks deterministic SPEC targets and protocol invariants:

| Test family | What is checked |
|---|---|
| Case A | `x1*x2*x3`, edge challenges `r=0`, `r=1`, and full challenge chain `(5,7,11) -> 385`. |
| Case B | `x1*x2`, `x2*x3`, and combined term `x1*x2 + x2*x3 -> [1,3,5]`. |
| Structural invariants | `s(0)+s(1)` equals pre-round claim, updated table claim equals `s(r)`, table size halves exactly. |

## Synthesis Summary

Live Vitis HLS synthesis was run on NYU ECE servers with Vitis HLS 2023.2. The detailed tables and report excerpts are in [`docs/verification/synthesis-summary.md`](docs/verification/synthesis-summary.md).

| Implementation | Target part | Clock target | Architecture |
|---|---:|---:|---|
| `hls/` | PYNQ-Z2 `xc7z020clg400-1` | 100 MHz | Single PE, scratchpad, shared modular multiplier, BRAM + m_axi APIs. |
| `hls_rfsoc/` | RFSoC 4x2 `xczu48dr-ffvg1517-2-e` | 200 MHz | 8 PEs, 16 scratchpad banks, tree reduction, BRAM API. |

## Running the Submission

On the NYU ECE server after VPN:

```bash
# PYNQ-Z2 implementation
cd ~/zkphire/hls
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl

# RFSoC implementation
cd ~/zkphire/hls_rfsoc
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl
```

## Paper Parity and Limitations

The RFSoC implementation is the main paper-parity design: it implements multiple PEs, banked scratchpad buffering, and a reduction tree. The PYNQ-Z2 design is intentionally smaller because its DSP budget is insufficient for a full zkPHIRE-like datapath over BN254 using direct `ap_uint<512> % p` reduction.

Documented limitations are in [`docs/paper-parity-plan.md`](docs/paper-parity-plan.md), including approaches that were evaluated but not retained (Barrett reduction on PYNQ-Z2, fabric-bound modulo, and Montgomery as a non-drop-in change for this codebase).
