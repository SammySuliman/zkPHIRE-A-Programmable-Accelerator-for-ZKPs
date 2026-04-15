# zkPHIRE SumCheck FPGA – Development Specification v2.0

> **Source of truth:** zkPHIRE: A Programmable Accelerator for ZKPs over HIgh-degRee, Expressive Gates (HPCA 2026), especially Section III and Figures 1 and 3.
>
> **Current intent:** build a verification-first reference flow for one programmable SumCheck round, then map that exact flow into a small Vitis HLS core for a Zynq/PYNQ-Z2 class target.

## 1. Project Intent

This project is not trying to implement an entire prover up front.

The immediate goal is narrower and more useful:

1. Define a mathematically clean golden model for one SumCheck round.
2. Verify the round logic aggressively in Python before touching HLS.
3. Implement the same one-round datapath in HLS.
4. Reuse that one-round datapath iteratively across rounds to recover full SumCheck behavior.

The first hardware milestone is therefore:

- one programmable SumCheck round core
- parameterized by term degree `d`
- operating on constituent MLE tables
- producing:
  - `d + 1` round samples
  - halved updated MLE tables for the next round

This aligns with the paper’s core dataflow:

- pair values from each constituent MLE
- extend each pair to `Xi = 0..d`
- multiply per term
- sum down the table
- update MLE tables with verifier challenge `r`

## 2. Scope and Non-Goals

### In scope now

- BN254 field arithmetic
- one-round SumCheck datapath
- single-term and multi-term software verification
- explicit table-halving behavior
- deterministic and randomized regression tests
- HLS implementation of the one-round core

### Out of scope for the first implementation

- transcript hashing inside hardware
- full HyperPlonk integration
- multi-PE scheduling from day one
- AXI/DMA system integration before C-sim correctness is stable

For this stage, the verifier challenge `r_i` is an external input from Python, the HLS testbench, or software on the host.

## 3. Mathematical Model

We represent the target polynomial as a sum of product terms:

```text
f(X) = sum_t term_t(X)
term_t(X) = prod_j m_{t,j}(X)
```

where each `m_{t,j}` is a multilinear polynomial stored as an MLE table.

### Important distinction

- **Term degree** = number of multiplicative factors in one term.
- **Round polynomial degree** = maximum term degree active in that round.

Examples:

- `x1 * x2 * x3` has one term of degree 3.
- `x1*x2 + x2*x3` has two terms, each of degree 2.

For the first HLS implementation, each invocation should assume a common configured degree `d` for the active terms being processed. Multi-term compositions are formed by summing per-term round contributions in software or in a thin controller layer.

## 4. Table Ordering Convention

The golden model and all generated vectors must use one consistent ordering.

### Current round variable is fastest-varying

At round `i`, the current variable `X_i` is represented by adjacent entries:

```text
(table[2k], table[2k+1]) = (value at Xi=0, value at Xi=1)
```

for a fixed assignment of the remaining variables.

### Example for three variables

For round 1 with `X1` as the active variable, the order is:

```text
[000, 100, 010, 110, 001, 101, 011, 111]
```

so `X1` toggles fastest.

After updating with challenge `r1`, the table halves. In round 2, the new adjacent pairs correspond to `X2 = 0/1`; after that, round 3 pairs correspond to `X3 = 0/1`.

This convention is mandatory for:

- the Python golden model
- test vector generation
- HLS input/output checking

## 5. One-Round Dataflow

For one term of degree `d`, and for one SumCheck round:

1. Read one pair `(f0, f1)` from each constituent MLE table.
2. Extend each pair to `d + 1` evaluations using the affine multilinear rule.
3. Multiply the per-factor extensions pointwise.
4. Accumulate these products over all row pairs to obtain the term’s round samples.
5. Update each constituent MLE table with the verifier challenge `r`.
6. Repeat across terms and sum the term samples to form the full round polynomial.

### Shared affine rule

The same rule is used for both extension and update:

```text
line_eval(f0, f1, z) = f0 * (1 - z) + f1 * z
                      = f0 + (f1 - f0) * z
```

### Round equations

For one term `term_t = prod_j m_{t,j}`, define:

```text
ext_{t,j,pair}(x) = line_eval(f0, f1, x),  for x in {0, 1, ..., d}
prod_{t,pair}(x)  = prod_j ext_{t,j,pair}(x)
s_t(x)            = sum_pair prod_{t,pair}(x)
```

Then the full round polynomial is:

```text
s(x) = sum_t s_t(x)
```

and each constituent MLE table updates as:

```text
m'_{t,j}[pair] = line_eval(f0, f1, r)
```

## 6. Invariants That Must Hold

These are the invariants the project must preserve at every stage.

### Round invariants

For any term or composition:

1. `s(0) + s(1)` equals the current claimed sum over the full table.
2. Updating all constituent MLE tables with challenge `r` yields a new claim equal to `s(r)`.
3. Every update halves the table size exactly.
4. The sampled values `s(0)..s(d)` interpolate back to the same polynomial evaluated at arbitrary `r`.

### Edge-case invariants

- `r = 0` selects the even entries from every pair.
- `r = 1` selects the odd entries from every pair.

### Protocol-chain invariant

If the same one-round logic is iterated until the tables collapse to scalars, the final scalar claim must match direct evaluation of the original multilinear factors at the challenge vector.

## 7. Golden Model Requirements

The current golden model lives at:

```text
golden_sumcheck.py
```

It is the software reference for all future HLS work.

### Required capabilities

- BN254 field arithmetic
- one-round evaluation for a single term
- table update for a single term
- multi-round chain verification
- deterministic hand-checkable examples
- randomized tests across degrees 2-6

### Required API shape

The reference model should continue to expose logic equivalent to:

- `line_eval(f0, f1, z)`
- `compute_round_evaluations(term_tables)`
- `update_tables(term_tables, challenge)`
- `sumcheck_round(term_tables, challenge)`
- round invariant checks
- protocol-chain checks

### Current validation command

```bash
python3 golden_sumcheck.py --test
```

This must pass before any HLS implementation work is considered trustworthy.

## 8. Deterministic Regression Targets

These examples are small enough to verify by hand and must remain stable.

### Case A: Monomial sanity check

```text
f(x1, x2, x3) = x1 * x2 * x3
challenges = (r1, r2, r3) = (5, 7, 11)
```

Expected round behavior:

```text
s1(t) = t                  -> samples [0, 1, 2, 3]
s2(t) = 5t                 -> samples [0, 5, 10, 15]
s3(t) = 35t                -> samples [0, 35, 70, 105]
final value = 5 * 7 * 11 = 385
```

### Case B: Two-term composition sanity check

```text
f(x1, x2, x3) = x1*x2 + x2*x3
challenges = (r1, r2, r3) = (5, 7, 11)
```

Expected round behavior:

```text
s1(t) = 1 + 2t             -> samples [1, 3, 5]
s2(t) = 11t                -> samples [0, 11, 22]
s3(t) = 35 + 7t            -> samples [35, 42, 49]
final value = 112
```

This case is especially useful because:

- it exercises multi-term accumulation
- both terms are degree 2
- the intermediate claims are small and hand-checkable

### Case C: Figure 1 style degree-4 term

Use one degree-4 product term over size-8 tables, as in the existing Python demo, to ensure the HLS core is exercised beyond the degree-2 toy cases.

## 9. Development Flow

The implementation should proceed in this order.

### Phase 0: Golden model and proofs of correctness

Deliver:

- stable `golden_sumcheck.py`
- deterministic regressions
- randomized regressions

Pass criteria:

- `python3 golden_sumcheck.py --test` passes
- hand-derived examples match the printed intermediate tables and claims

### Phase 1: Functional HLS one-round core

Implement only the minimal functional core:

- read constituent MLE tables for one term
- compute `d + 1` round samples
- compute updated halved tables

No scheduler sophistication is required yet. A direct array-based implementation is acceptable if it is bit-exact with the golden model.

Pass criteria:

- HLS C-sim matches golden model exactly on:
  - round samples
  - updated constituent tables

### Phase 2: Multi-term accumulation

Add either:

- a controller that invokes the one-term round core per term and accumulates samples, or
- a top-level software/testbench flow that does the same composition step around the HLS block

Pass criteria:

- `x1*x2 + x2*x3` matches the deterministic regression exactly

### Phase 3: Paper-aligned optimizations

Only after functional correctness is stable:

- scratchpad buffering
- multiple product lanes
- pipelining
- dataflow pragmas
- tiling and scheduling

Pass criteria:

- bit-exact behavior remains unchanged
- synthesis begins approaching board constraints

### Phase 4: PYNQ / board integration

After C-sim and synthesis are stable:

- DMA or memory-mapped input path
- AXI-Lite configuration for challenge and degree
- host-side invocation

## 10. Core HLS Modules

These are still the intended modules, but they should be introduced in the phase order above rather than all at once.

### `types.hpp`

- BN254 field element representation
- compile-time degree/table configuration

### `update_unit.hpp`

Function:

```text
update(f0, f1, r) = f0*(1-r) + f1*r
```

### `extension_engine.hpp`

Function:

```text
extend_pair(f0, f1, degree) -> [value at 0, 1, ..., degree]
```

Note: extension uses the affine rule over integer points `0..degree`. It does not take the verifier challenge as input.

### `product_lane.hpp`

Function:

- multiply one extension value from each constituent factor of a term

### `accumulator.hpp`

Function:

- accumulate the per-point products into `d + 1` round registers

### `sumcheck_top.cpp`

Function:

- orchestrate load -> extend -> product -> accumulate -> update for one term and one round

### Scratchpad / scheduling modules

These remain valid paper-aligned optimizations, but they are not a prerequisite for the first functional HLS milestone.

## 11. Vector Generation Requirements

When vector generation is added, each test case must include enough information to verify both outputs of the one-round core.

### Required artifacts per case

- constituent MLE input tables
- configured degree `d`
- challenge `r`
- expected round samples `s(0..d)`
- expected updated constituent tables

Only storing `expected_s.txt` is not sufficient; the updated tables must also be checked.

## 12. FPGA Target Constraints

Current target board:

- PYNQ-Z2 / `xc7z020clg400-1`

Targeted design envelope:

- degree up to 6 for the first milestone
- clock target: 100 MHz
- DSP budget goal: `< 180`
- BRAM budget goal: `< 80%`

These are optimization targets, not reasons to skip correctness work.

## 13. Success Criteria

The project is considered on track when all of the following are true:

1. The Python golden model is the unambiguous reference for one-round behavior.
2. Deterministic examples such as `x1*x2*x3` and `x1*x2 + x2*x3` match by hand and in code.
3. HLS C-sim matches the golden model bit-exactly for both round samples and updated tables.
4. Repeated HLS invocation reproduces correct multi-round claims.
5. Only after the above is stable do we optimize toward scratchpads, lanes, and board-level integration.
