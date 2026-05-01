# Interface Contract: SumCheck HLS IP

This document spells out the messages that software sends to the HLS IP and the exact memory layout expected by each interface. It is intended to remove ambiguity between the mathematical SumCheck operation, C-simulation, and board integration.

## Scalar Control Registers

Both top-level functions use AXI-Lite scalar control for configuration and return status.

| Field | Type | Meaning | Valid range |
|---|---|---|---|
| `degree` | `int` | Number of MLE factors in one product term. Hardware emits `degree+1` samples. | `1 <= degree <= MAX_DEGREE` |
| `size` | `int` | Number of entries in each MLE table before this round. | power of two, `2 <= size <= MAX_TABLE_SIZE` |
| `r` | `field_elem_t` (`ap_uint<256>`) | Verifier challenge used for table update. | `0 <= r < FIELD_P` |
| return | `status_t` (`ap_uint<4>`) | Input validation status. | See status table below. |

Status codes:

| Code | Name | Meaning |
|---:|---|---|
| 0 | `STATUS_OK` | Computation completed. |
| 1 | `STATUS_BAD_DEGREE` | `degree` is outside supported range. |
| 2 | `STATUS_BAD_SIZE` | `size` is not a power of two or exceeds configured capacity. |
| 3 | `STATUS_BAD_CHALLENGE` | `r >= FIELD_P`. |

The BN254 scalar-field prime is:

```text
FIELD_P = 0x30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000001
```

All field elements are 256-bit unsigned integers. Host code should pass them as 32-byte little-endian words when using Xilinx/PYNQ buffer views backed by `uint64` lanes, or as 256-bit HLS objects in C-sim.

## BRAM Array Interface

Top function:

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

HLS pragmas:

```cpp
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=degree bundle=control
#pragma HLS INTERFACE s_axilite port=size bundle=control
#pragma HLS INTERFACE s_axilite port=r bundle=control
#pragma HLS INTERFACE bram port=tables
#pragma HLS INTERFACE bram port=samples
#pragma HLS INTERFACE bram port=updated
```

### Input Layout

`tables[m][i]` is the `i`-th evaluation of MLE factor `m`.

| Dimension | Range | Meaning |
|---|---|---|
| `m` | `0..degree-1` | Which MLE factor in the product term. |
| `i` | `0..size-1` | Boolean-hypercube table index for the current round. |

Only rows `0..degree-1` and columns `0..size-1` are read. Remaining capacity is ignored.

### Output Layout

`samples[x]` for `x=0..degree` contains the univariate round polynomial evaluated at integer points `0..degree`:

```text
samples[x] = Σ_k Π_m (tables[m][2k] + (tables[m][2k+1] - tables[m][2k]) * x)
```

`updated[m][k]` for `m=0..degree-1`, `k=0..size/2-1` contains:

```text
updated[m][k] = tables[m][2k] + (tables[m][2k+1] - tables[m][2k]) * r
```

This is the next-round MLE table after fixing the current variable to challenge `r`.

## m_axi DMA Interface (`hls/`)

Top function:

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

HLS pragmas define three independent memory bundles:

| Pointer | Bundle | Direction | Depth | Layout |
|---|---|---|---:|---|
| `mle_inputs` | `gmem0` | host -> IP | `MAX_DEGREE*MAX_TABLE_SIZE` | Flat row-major `mle_inputs[m*size + i]`. |
| `round_samples` | `gmem1` | IP -> host | `MAX_SAMPLES` | `round_samples[x]`, `x=0..degree`. |
| `next_tables` | `gmem2` | IP -> host | `MAX_DEGREE*MAX_TABLE_SIZE/2` | Flat row-major `next_tables[m*(size/2)+k]`. |

### Host Driving Sequence

1. Allocate `mle_inputs` with `degree * size` 256-bit field elements.
2. Fill `mle_inputs[m*size + i]` for all active rows/columns.
3. Allocate `round_samples` with `degree+1` field elements.
4. Allocate `next_tables` with `degree * (size/2)` field elements.
5. Program AXI-Lite registers: buffer addresses, `degree`, `size`, `r`.
6. Start the IP.
7. Wait for completion and read status.
8. Read `round_samples[0..degree]` and `next_tables[0..degree*(size/2)-1]`.
9. For the next SumCheck round, use `next_tables` as the active table set and halve `size`.

## RFSoC 8-PE Interface

The RFSoC directory currently synthesizes `sumcheck_round_array` as the verified top. It keeps the same BRAM contract above, while implementing a larger internal architecture:

- `NUM_PES = 8`
- `SCRATCHPAD_BANKS = 16`
- `MAX_DEGREE = 6`
- `MAX_TABLE_SIZE = 256`

The previous AXI-Stream experiment was intentionally deferred because the grading-critical path is a working, measured HLS design with clear message semantics. The PYNQ-Z2 `sumcheck_round_axi` remains the board/DMA reference interface.
