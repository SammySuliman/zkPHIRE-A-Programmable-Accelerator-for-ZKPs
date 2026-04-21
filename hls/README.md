# zkPHIRE SumCheck HLS Core

This directory implements the first functional Vitis HLS milestone from `SPEC.md`: one programmable SumCheck round for a single product term over BN254 field elements.

## What The Core Does

For a term of degree `d` with `d` constituent MLE tables, the core:

1. reads adjacent pairs `(f0, f1)` for the active round variable,
2. extends each pair at points `0..d` with `f0 + (f1 - f0) * x`,
3. multiplies extensions across the `d` factors,
4. accumulates the `d + 1` round samples, and
5. emits each halved updated MLE table using the verifier challenge `r`.

The implementation supports `degree <= 6` and `table_size <= 1024` by default. These limits live in `include/types.hpp`.

## APIs

`sumcheck_round_array` is the convenient C-sim/testbench API. It accepts full input tables and returns both required outputs:

```cpp
status = sumcheck_round_array(input_tables, challenge, degree, table_size, samples, updated_tables);
```

`sumcheck_round_axis` is the IP-facing streaming top level for Vitis HLS:

```cpp
status = sumcheck_round_axis(pairs_in, samples_out, updates_out, challenge, degree, table_size);
```

Input stream order is pair-major, then factor-major:

```text
for pair in 0..table_size/2-1:
  for factor in 0..degree-1:
    write table[factor][2*pair]
    write table[factor][2*pair + 1]
```

Output streams are separated for host convenience:

- `samples_out`: `degree + 1` field words, with TLAST on `s(degree)`.
- `updates_out`: `degree * table_size / 2` field words, pair-major/factor-major, with TLAST on the final update.

The stream separation lets software read updated tables as they are produced while samples finish accumulating.

## Local Verification Without Vitis

The testbench uses Vitis `ap_uint<256>` when available, Boost multiprecision when available, and otherwise a dependency-free 64-bit prime fallback so the datapath/control logic can still be checked on minimal machines:

```bash
make -C hls test
```

This verifies:

- `x1*x2*x3` multi-round deterministic chain,
- `x1*x2 + x2*x3` multi-term composition around the one-term core,
- the Figure-1-style degree-4 regression,
- AXI-stream wrapper ordering and TLAST behavior,
- modular wraparound near the active C++ field modulus.

BN254-width C++ verification requires either Vitis headers or Boost headers. The Python golden model always verifies against BN254.

Also keep the Python oracle green:

```bash
python3 golden_sumcheck.py --test
```

## Verification With Vitis HLS

On a machine with Vitis HLS installed and available on `PATH`:

```bash
make -C hls vitis-csim
```

To run C synthesis and export an IP catalog package for the PYNQ-Z2 target:

```bash
make -C hls vitis-csynth
```

The script targets `xc7z020clg400-1` at 100 MHz, matching `SPEC.md`.

## Test Vector Generation

Generate JSON vectors from the Python golden model:

```bash
python3 tools/generate_hls_vectors.py --pretty
```

Each vector includes input MLE tables, degree, challenge, expected round samples, and expected updated tables, so a hardware or host-side test can check both outputs required by the spec.

## Notes For Host Integration

- Field elements are 256-bit AXI words and must be reduced modulo BN254 before being sent.
- The verifier challenge is passed through AXI-Lite as a 256-bit scalar.
- For multi-term polynomials, invoke the one-term core once per term and add the returned `samples_out` values in software or a thin controller. Updated tables remain per term.
- Invalid `degree` or `table_size` returns a nonzero status and does not consume streams.
