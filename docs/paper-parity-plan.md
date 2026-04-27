# zkPHIRE Paper Parity — Completion Plan

> **Goal:** Bring our PYNQ-Z2 SumCheck HLS implementation to paper parity with the zkPHIRE HPCA 2026 architecture (Sections III–IV), within board constraints.

**Architecture:** Montgomery modular multiplication replaces the DSP-heavy `ap_uint<512> %` divider. Scratchpad BRAM buffers cache MLE tiles across terms. Dual processing elements (PEs) demonstrate paper's parallelism at PYNQ-Z2 scale. Fused update+extension pipeline matches paper's round-2+ dataflow.

**Tech Stack:** Vitis HLS 2023.2, PYNQ-Z2 (xc7z020clg400-1), C++17, ap_uint<256>, Python 3.11 golden model.

---

## Phase 3c: Montgomery Modular Multiplication

**Impact:** Fixes DSP overage (237→~60 DSPs). The paper and zkSpeed both use Montgomery.

### Montgomery reduction for BN254 scalar field

```
R = 2^256                    (Montgomery radix, > p)
R^2 mod p                    (precomputed, for domain conversion)
p' = -p^{-1} mod R           (precomputed, Montgomery constant)
```

**Montgomery multiply:**
```
function mon_mul(a', b'):      // a', b' in Montgomery domain
    T = a' * b'                 // 512-bit product
    m = (T * p') mod R          // reduction factor (low 256 bits)
    t = (T + m * p) >> 256      // reduce and shift
    if t >= p: t -= p           // final conditional subtraction
    return t                    // result = a' * b' * R^{-1} mod p
```

**No division!** Only 256×256 multiplies + shifts + conditional subtract.

**Files:**
- Modify: `hls/include/field_arithmetic.hpp` — replace `mod_mul` with Montgomery
- Add: Montgomery constants (R^2 mod p, p')

**Estimated DSP per multiply:** 8-12 (3 multiplies × ~3 DSPs each) vs ~200 for `%` divider.

**Verification:** Run C-sim (8/8 tests), check DSP count in synthesis report (<100 DSPs).

---

## Phase 3d: Scratchpad Buffering

**Impact:** Paper parity for data reuse. Paper uses 16 banked scratchpads; we implement 4.

### Design

- 4 BRAM banks (280 available, currently using 0)
- Each bank: MAX_TABLE_SIZE/2 × 256 bits = 16 Kb per bank
- Store MLE tiles for reuse across terms in multi-term polynomials
- Banked to match PE count (2 PEs → 4 banks = 2 banks/PE)

**Files:**
- Modify: `hls/include/types.hpp` — add `SCRATCHPAD_BANKS`, `SCRATCHPAD_DEPTH`
- Add: `hls/include/scratchpad.hpp` — load/store tile functions
- Modify: `hls/src/sumcheck_top.cpp` — integrate scratchpad into datapath

**Verification:** BRAM utilization appears in synthesis report (previously 0).

---

## Phase 3e: Fused Update + Extension Pipeline

**Impact:** Matches paper's Figure 4 dataflow. In rounds 2+, MLE Update and Extension are pipelined together.

### Current design:
```
Round 1: Read 2 values → EE → PL → accumulate → (separate pass) Update tables
```

### Paper design (after Montgomery fixes DSP):
```
Round 1:   Read 2 values/MLE → EE → PL → accumulate
Round 2+:  Read 4 values/MLE → Update (pipelined) → EE → PL → accumulate
           (Updated values written to FIFO for next round)
```

### Implementation:
- `update_unit.hpp`: add 4-input update mode (`update4`)
- `sumcheck_top.cpp`: add `#pragma HLS DATAFLOW` between load/update/extend/product/accumulate stages
- Round detection: use `round_num` input parameter

**Files:**
- Modify: `hls/include/update_unit.hpp` — add `update4` for 4-input mode
- Modify: `hls/src/sumcheck_top.cpp` — DATAFLOW pragma, round-dependent path

---

## Phase 3f: Dual Processing Element

**Impact:** Demonstrates paper's multi-PE parallelism at reduced scale.

### Design

- 2 PEs, each with: Extension Engine + Product Lane + Accumulator
- Each PE processes different terms (or different tile rows) in parallel
- Shared Montgomery multiplier via HLS resource sharing
- `#pragma HLS DATAFLOW` between PEs

### Resource budget (after Montgomery):
- DSP: 2 PEs × ~40 DSPs/PE = ~80 DSPs (36% of 220)
- BRAM: 4 scratchpad banks = ~64 Kb (negligible vs 280 BRAMs)
- FF/LUT: Well within budget

**Files:**
- Modify: `hls/src/sumcheck_top.cpp` — dual-PE instantiation with DATAFLOW

---

## Phase 4: PYNQ Board Integration

**Impact:** End-to-end demo on actual PYNQ-Z2 hardware.

### Deliverables

1. **AXI IP export** — `run_axi.tcl` (already works)
2. **PYNQ overlay notebook** — Jupyter notebook to load bitstream + invoke hardware
3. **Host-side Python driver** — loops SumCheck rounds, feeds challenges
4. **Demonstration** — Run Case A `x1*x2*x3` chain on PYNQ-Z2

**Files:**
- Add: `hls/pynq/zkphire_overlay.ipynb` — PYNQ overlay notebook
- Add: `hls/pynq/sumcheck_driver.py` — host-side round loop
- Add: `hls/pynq/build_overlay.tcl` — Vivado block design for PYNQ-Z2

---

## Documented Limitations

| Limitation | Reason |
|-----------|--------|
| Barrett reduction | Made DSP worse (683 vs 458); `%` maps to sequential divider which is more DSP-efficient |
| `BIND_OP impl=fabric` | Not supported on xc7z020; fabric modulo requires 7-series specific IP |
| >2 PEs | DSP budget of 220 limits to ~2 PEs even after Montgomery |
| Degree > 6 | Requires more registers than budget allows; paper supports up to degree 31 via scratchpads |
| Full HyperPlonk (ZeroCheck, PermCheck, OpenCheck) | Out of scope; one-round SumCheck is the project milestone per SPEC §2 |
| Multifunction Forest | Not needed for single-round SumCheck; paper's unified reduction tree handles multi-protocol |
| 381-bit BLS12-381 field | Paper uses this for MSMs; project uses BN254 for MLEs only |
| 16 PEs (paper) | PYNQ-Z2 DSP budget allows 2 PEs; paper's ASIC has hundreds of DSPs |
| Custom gate scheduling | Paper's graph-based scheduler for arbitrary gate types is out of scope |

## Execution Flow

Each phase follows: **Implement → C-sim verify → Synthesize → Check DSP/BRAM → Commit**

```
Phase 3c (Montgomery) → C-sim → Synth → Check DSP < 100
    ↓
Phase 3d (Scratchpad) → C-sim → Synth → Check BRAM > 0
    ↓
Phase 3e (Fused pipeline) → C-sim → Synth → Verify Fmax
    ↓
Phase 3f (Dual PE) → C-sim → Synth → Check DSP < 120
    ↓
Phase 4 (PYNQ overlay) → Board test
```
