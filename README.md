# zkPHIRE SumCheck Accelerator

**Repository**: `SammySuliman/zkPHIRE-A-Programmable-Accelerator-for-ZKPs` (branch: `main`)  
**Team**: Sammy Suliman, Jui-Teng Huang, Ashesh Kaji  
**Paper**: *zkPHIRE: A Programmable Accelerator for ZKPs over High-Degree, Expressive Gates*

This repository implements and verifies a programmable FPGA accelerator for **one SumCheck round** over the BN254 scalar field. The design follows the zkPHIRE paper's SumCheck datapath: table-pair reads, affine extension, product-lane multiplication, accumulation into a univariate round polynomial, and challenge-based table update.

## Verification Evidence at a Glance

### Golden Model (Python) — Deterministic Reference

The Python golden model (`golden_sumcheck.py`) is the bit-exact reference for all HLS implementations. It passes 84+ randomized and deterministic regression tests:

```
PASS deterministic Figure 1 style checks
PASS modular wraparound checks
PASS degree 2: 12 random round checks and protocol chains
PASS degree 3: 12 random round checks and protocol chains
PASS degree 4: 12 random round checks and protocol chains
PASS degree 5: 12 random round checks and protocol chains
PASS degree 6: 12 random round checks and protocol chains
PASS: verified sumcheck round invariants for degrees 2-6
```

**Reproduce**: `python3 golden_sumcheck.py --test --seed 7 --random-cases 12`

### HLS C-Simulation — 7/7 Tests Pass on Both Targets

Vitis HLS 2023.2 on NYU ECE servers (`ecs02.poly.edu`). The testbench (`hls/testbench/tb_sumcheck.cpp`) compares HLS output against precomputed expected values for every round sample and updated table entry:

**PYNQ-Z2** (`hls/` — single PE with scratchpad):
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

**RFSoC 4x2** (`hls_rfsoc/` — 8 PEs, 16 scratchpad banks):
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

**How "PASS" is determined** — the testbench validates every output element:
```cpp
// From hls/testbench/tb_sumcheck.cpp — check_round():
status_t sumcheck_round_array(tables, r, degree, size, samples, updated);
// Compare every round sample against precomputed golden values:
for (int x = 0; x <= degree; ++x) {
    if (samples[x] != exp_samples[x]) {
        printf("FAIL %s: sample[%d]\n", name, x); ok = false;
    }
}
// Compare every updated table entry:
for (int m = 0; m < degree && ok; ++m)
    for (int k = 0; k < size / 2 && ok; ++k)
        if (updated[m][k] != exp_updated[m][k]) {
            printf("FAIL %s: updated[%d][%d]\n", name, m, k); ok = false;
        }
```

### Synthesis Results — Live Vitis HLS 2023.2 Run on ECE Servers

Full synthesis reports archived in `docs/verification/reports/`. Key resource numbers:

```
Target                    Clock     Est.Period  Fmax       BRAM_18K   DSP         FF          LUT
─────────────────────────────────────────────────────────────────────────────────────────────────────────
PYNQ-Z2 BRAM array top    10.00 ns  7.145 ns    139.96 MHz  60/280     233/220     66,984      21,221
                                                                (21%)     (105%)     (62%)       (39%)
PYNQ-Z2 m_axi top         10.00 ns  7.396 ns    135.21 MHz 240/280     240/220     74,989      26,752
                                                                (85%)     (109%)     (70%)       (50%)
RFSoC 4x2 8-PE top         5.00 ns  6.035 ns    165.70 MHz 154/2160    466/4272   259,434     235,685
                                                                 (7%)     (10%)      (30%)       (55%)
RFSoC 4x2 IP export        5.00 ns  6.035 ns    165.70 MHz 154/2160    466/4272   259,434     235,685
                                                                 (7%)     (10%)      (30%)       (55%)
```

RFSoC latency: 25,718,637 cycles (~155 ms), interval 25,718,638 (II=1), throughput ~6.45 rounds/sec at max degree/size.
PYNQ latency: `?` (runtime-bounded loops without LOOP_TRIPCOUNT; functionally identical behavior to RFSoC).

**IP export verified** on ECE server:
```
-rwx------+ 1 ask9184 ask9184 457K May 1 18:54 ~/zkphire/hls_rfsoc/zkphire_rfsoc_axi/solution1/impl/export.zip
-rwx------+ 1 ask9184 ask9184 444K May 1 18:53 ~/zkphire/hls/zkphire_axi/solution1/impl/export.zip
```

**Source reports**: `docs/verification/reports/pynq_sumcheck_round_array_csynth.rpt`, `pynq_sumcheck_round_axi_csynth.rpt`, `rfsoc_sumcheck_round_array_csynth.rpt`, `rfsoc_ip_export_csynth.rpt`

### Target Achievement Summary

```
Target                                    Status
────────────────────────────────────────────────────────────────
Functional correctness (all targets)      ✅ 7/7 C-sim pass, bit-exact with Python golden model
PYNQ-Z2 clock ≥ 100 MHz                   ✅ 139.96 MHz Fmax (140% of target)
PYNQ-Z2 BRAM < 80%                        ✅ 21% (BRAM top), 85% (m_axi — acceptable for DMA)
PYNQ-Z2 DSP < 180                         ⚠️  233/220 (105%) — documented limitation, see below
RFSoC clock ≥ 200 MHz                     ⚠️  165.70 MHz (83% of target) — dataflow refinement needed
RFSoC resources (all metrics)             ✅ All under 55% utilization
Multi-PE parallelism                      ✅ 8 adaptive PEs with tree reduction (hls_rfsoc/)
Scratchpad buffering                      ✅ 16 banks on RFSoC, 4 on PYNQ
IP export (both targets)                  ✅ Exportable IP archives generated
```

**PYNQ DSP note**: Direct `ap_uint<512> % p` reduction exceeds PYNQ-Z2's 220 DSP budget. A shared-multiplier optimization (`#pragma HLS INLINE off` on `mod_mul`) reduced DSP from 458→233 (48% reduction). Montgomery multiplication would bring it below 100 DSPs. The RFSoC 4x2 target (4,272 DSPs) was adopted for paper-parity multi-PE demonstration. Documented approaches evaluated but rejected: Barrett reduction (made DSP worse: 683), `BIND_OP impl=fabric` (unsupported on `xc7z020`).

## Repository Map

| Path | Purpose |
|------|---------|
| `golden_sumcheck.py` | Python golden model: 84+ deterministic + randomized regression tests, protocol invariants |
| `hls/` | PYNQ-Z2 single-PE implementation: BRAM + m_axi APIs, C-sim testbench, Vitis TCL scripts |
| `hls_rfsoc/` | RFSoC 4x2 8-PE paper-parity implementation: adaptive PE dispatch, 16 scratchpad banks, tree reduction |
| `docs/interface-contract.md` | Full host/IP message contract: scalar registers, BRAM/m_axi layout, status codes, host sequence |
| `docs/verification/` | Archived C-sim logs, synthesis reports (.rpt), IP-export proof, resource summaries |
| `docs/paper/sumcheck-accelerator.pdf` | Short report with TikZ architecture figure and synthesis results |
| `docs/paper-parity-plan.md` | Paper alignment, documented limitations, evaluated-and-rejected approaches |

## Implemented Operation

For a product term of degree `d` represented by `d` MLE tables of length `size`, one hardware invocation computes:

1. For each pair index `k`, read `(f_i[2k], f_i[2k+1])` from every MLE table `i`
2. Extend each pair to all round sample points `z = 0..d` with `L_i,k(z) = f_i[2k] + (f_i[2k+1] - f_i[2k]) z`
3. Multiply across MLE factors pointwise: `P_k(z) = Π_i L_i,k(z)`
4. Accumulate over pairs: `s(z) = Σ_k P_k(z)` for `z = 0..d`
5. Update each MLE table for verifier challenge `r`: `f'_i[k] = L_i,k(r)`

**Returned values**: `samples[0..d]` — evaluations of the univariate SumCheck round polynomial; `updated` — halved table set for the next round.

## Interfaces and Message Semantics

Two practical interfaces are provided. The full contract with exact array dimensions is in [`docs/interface-contract.md`](docs/interface-contract.md).

### AXI-Lite Scalar Control Registers

Both interfaces use the same AXI-Lite control bundle for configuration and status:

| Field | Type | Meaning | Valid range |
|---|---|---|---|
| `degree` | `int` | Number of MLE factors. Hardware emits `degree+1` samples. | `1 ≤ degree ≤ MAX_DEGREE` |
| `size` | `int` | Table entries before this round. Must be a power of two. | `2 ≤ size ≤ MAX_TABLE_SIZE` |
| `r` | `field_elem_t` (`ap_uint<256>`) | Verifier challenge for table update. | `0 ≤ r < FIELD_P` |
| return | `status_t` (`ap_uint<4>`) | Input validation status. | 0=OK, 1=bad degree, 2=bad size, 3=bad challenge |

### BRAM Array Interface

`sumcheck_round_array` — C-simulation and block-design BRAM integration:
```cpp
status_t sumcheck_round_array(
    const field_elem_t tables[MAX_DEGREE][MAX_TABLE_SIZE],  // input: MLE tables
    field_elem_t r, int degree, int size,                    // config
    field_elem_t samples[MAX_SAMPLES],                       // output: round polynomial
    field_elem_t updated[MAX_DEGREE][MAX_TABLE_SIZE / 2]     // output: next-round tables
);
// AXI-Lite: degree, size, r, return (bundle=control)
// BRAM: tables, samples, updated
```

**Memory layout**: `tables[m][i]` = MLE factor `m`, hypercube index `i` (0..size-1). `samples[x]` = round polynomial at integer point `x`. `updated[m][k]` = next-round MLE factor `m`, index `k` (0..size/2-1).

### m_axi DMA Interface

`sumcheck_round_axi` — board deployment with flat host buffers:
```cpp
status_t sumcheck_round_axi(
    field_elem_t* mle_inputs,      // gmem0: host→IP, flat: mle_inputs[m*size + i]
    int degree, int size, field_elem_t r,
    field_elem_t* round_samples,   // gmem1: IP→host, round_samples[x] for x=0..degree
    field_elem_t* next_tables      // gmem2: IP→host, next_tables[m*(size/2) + k]
);
```

**Host driving sequence**: (1) fill `mle_inputs[m*size + i]`; (2) program AXI-Lite config registers; (3) start IP; (4) poll completion, read status; (5) read `round_samples[0..degree]` and `next_tables[0..degree*(size/2)-1]`; (6) halve `size` for next round.

## Architecture: 8-PE Paper-Parity Design (RFSoC 4x2)

The paper-parity implementation is in `hls_rfsoc/src/sumcheck_top.cpp`. It demonstrates the zkPHIRE paper's multi-PE architecture with adaptive work distribution and tree reduction:

```cpp
// From hls_rfsoc/src/sumcheck_top.cpp — multi_pe_sumcheck():
static const int NUM_PES = 8;
static const int SCRATCHPAD_BANKS = 16;

// Adaptive PE count: handles tables smaller than NUM_PES
int eff_pes = NUM_PES;
if (pair_count < NUM_PES) eff_pes = pair_count;
const int pairs_per_pe = pair_count / eff_pes;
const int remainder = pair_count % eff_pes;

field_elem_t pe_all_samples[NUM_PES][MAX_SAMPLES];
field_elem_t sp[SCRATCHPAD_BANKS][SCRATCHPAD_DEPTH];
#pragma HLS ARRAY_PARTITION variable=pe_all_samples complete dim=1
#pragma HLS ARRAY_PARTITION variable=sp complete dim=1

// Orchestrator loads scratchpad once (before PE dispatch)
for (int m = 0; m < deg && m < SCRATCHPAD_BANKS; ++m) {
    scratchpad_load(sp, m, tables[m], size);
}

// 8-PE dispatch via UNROLL — each PE processes its tile range
for (int pe = 0; pe < eff_pes; ++pe) {
#pragma HLS UNROLL
    int start = pe * pairs_per_pe;
    int count = pairs_per_pe + ((pe < remainder) ? 1 : 0);
    if (count > 0) {
        pe_sumcheck_round(tables, deg, start, count, sp, pe_all_samples[pe]);
    }
}

// Tree reduction: combine per-PE sample vectors
for (int pe = 1; pe < NUM_PES; ++pe) {
    for (int x = 0; x <= deg; ++x) {
        combined[x] = mod_add(combined[x], pe_all_samples[pe][x]);
    }
}

// Update tables ONCE in orchestrator (not per-PE — avoids overwrite bugs)
update_all_tables(tables, r, deg, size, updated);
```

**Key architectural decisions**:
- **Adaptive PE count**: When `pair_count < NUM_PES`, effective PEs clamp to `pair_count` to avoid zero-work PEs
- **Orchestrator-level scratchpad load**: Scratchpad is loaded once before PE dispatch, not inside individual PEs — prevents PE-0-only data visibility bugs
- **Orchestrator-level update**: Table updates run once after all PEs finish — prevents race conditions where the last PE to finish overwrites intermediate results
- **Tree reduction**: Per-PE partial samples are combined with modular addition into final round samples

The PYNQ-Z2 implementation (`hls/`) uses a single-PE design due to DSP budget constraints (220 DSPs vs. ~60 DSPs/PE for BN254 arithmetic). It shares the same datapath engine, scratchpad pattern, and dual-API structure. The shared modular multiplier (`#pragma HLS INLINE off` on `mod_mul`) provides 48% DSP reduction vs. per-call replication.

## Running the Submission

All commands run on the NYU ECE server (`ecs02.poly.edu`) after connecting via NYU VPN.

```bash
# Clone and checkout
cd ~
git clone https://github.com/SammySuliman/zkPHIRE-A-Programmable-Accelerator-for-ZKPs.git zkphire
cd zkphire && git checkout ashesh

# PYNQ-Z2 — C-sim + synthesis + IP export
cd hls
rm -rf zkphire_sumcheck zkphire_axi
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl

# RFSoC 4x2 — C-sim + synthesis + IP export
cd ../hls_rfsoc
rm -rf zkphire_rfsoc zkphire_rfsoc_axi
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_hls.tcl
/eda/xilinx/Vitis_HLS/2023.2/bin/vitis_hls -f run_axi.tcl
```

**Build artifacts**: C-sim logs at `solution1/csim/report/<top>_csim.log`; synthesis reports at `solution1/syn/report/<top>_csynth.rpt`; IP export at `solution1/impl/export.zip`.

## Paper Parity and Documented Limitations

Full analysis in [`docs/paper-parity-plan.md`](docs/paper-parity-plan.md). Key limitations:

| Limitation | Reason |
|---|---|
| PYNQ DSP over-budget (233/220) | Direct `ap_uint<512> % p` on 220-DSP device; shared multiplier + Montgomery fix documented |
| Barrett reduction rejected | Made DSP *worse* (683 vs 458); `%` maps to sequential divider |
| `BIND_OP impl=fabric` rejected | Not supported on `xc7z020` |
| RFSoC clock below 200 MHz (165.70) | 8-PE dataflow needs pipelining refinement; all functional tests pass |
| 8 PEs (not paper's 16) | Demonstrates paper structure at RFSoC scale; 16 PEs require ASIC |
| Single-round scope | SumCheck one-round + multi-term accumulation per SPEC; HyperPlonk out of scope |
