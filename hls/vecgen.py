#!/usr/bin/env python3
"""
zkPHIRE HLS Test Vector Generator

Generates binary test vectors from the golden model for HLS C-simulation.
Each test case produces:
  - tables.bin       : input MLE tables (degree × size field elements)
  - config.bin       : degree, size, challenge r (as 32-bit ints, then 256-bit r)
  - samples_exp.bin  : expected round samples s(0..d)
  - updated_exp.bin  : expected updated tables (degree × size/2 field elements)

Usage:
  python3 vecgen.py                    # generate default test vectors
  python3 vecgen.py --all              # generate full regression suite
  python3 vecgen.py --case monomial    # Case A only
  python3 vecgen.py --case figure1     # Case C only
"""

from __future__ import annotations

import argparse
import random
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, Sequence

# ---------------------------------------------------------------------------
# Golden model (copied from golden_sumcheck.py for standalone use)
# ---------------------------------------------------------------------------

FIELD_P = 21888242871839275222246405745257275088548364400416034343698204186575808495617
FIELD_BYTES = 32  # 256 bits


def field(x: int) -> int:
    return x % FIELD_P


def mod_add(a: int, b: int) -> int:
    return (a + b) % FIELD_P


def mod_sub(a: int, b: int) -> int:
    return (a - b) % FIELD_P


def mod_mul(a: int, b: int) -> int:
    return (a * b) % FIELD_P


def affine_line_eval(f0: int, f1: int, point: int) -> int:
    point = field(point)
    return mod_add(f0, mod_mul(mod_sub(f1, f0), point))


def extend_pair(f0: int, f1: int, degree: int) -> tuple[int, ...]:
    return tuple(affine_line_eval(f0, f1, x) for x in range(degree + 1))


@dataclass
class MLETable:
    values: tuple[int, ...]

    def __len__(self) -> int:
        return len(self.values)

    def __getitem__(self, idx: int) -> int:
        return self.values[idx]

    def pairs(self) -> Iterator[tuple[int, int]]:
        for idx in range(0, len(self.values), 2):
            yield self.values[idx], self.values[idx + 1]


def validate_term(mles: Sequence[MLETable]) -> tuple[int, int]:
    if not mles:
        raise ValueError("expected at least one MLE table")
    size = len(mles[0])
    for idx, mle in enumerate(mles[1:], start=1):
        if len(mle) != size:
            raise ValueError(
                f"MLE table {idx} has size {len(mle)}, expected common size {size}"
            )
    return len(mles), size


def compute_round_evaluations(mles: Sequence[MLETable]) -> tuple[int, ...]:
    degree, size = validate_term(mles)
    pair_count = size // 2
    all_extensions = tuple(
        tuple(extend_pair(f0, f1, degree) for f0, f1 in mle.pairs()) for mle in mles
    )
    accumulators = [0] * (degree + 1)
    for pair_idx in range(pair_count):
        lane_products = [1] * (degree + 1)
        for mle_idx in range(degree):
            extensions = all_extensions[mle_idx][pair_idx]
            for x, value in enumerate(extensions):
                lane_products[x] = mod_mul(lane_products[x], value)
        for x, product in enumerate(lane_products):
            accumulators[x] = mod_add(accumulators[x], product)
    return tuple(accumulators)


def update_tables(mles: Sequence[MLETable], challenge: int) -> tuple[MLETable, ...]:
    return tuple(
        MLETable(
            tuple(affine_line_eval(f0, f1, challenge) for f0, f1 in mle.pairs())
        )
        for mle in mles
    )


# ---------------------------------------------------------------------------
# Binary encoding helpers
# ---------------------------------------------------------------------------

def _to_bytes(val: int, nbytes: int = FIELD_BYTES) -> bytes:
    """Serialize a field element as big-endian bytes."""
    return val.to_bytes(nbytes, "big")


def _to_u32(val: int) -> bytes:
    """Serialize a uint32 as little-endian bytes (HLS convention)."""
    return struct.pack("<I", val & 0xFFFFFFFF)


def write_field_elems(path: Path, values: Iterable[int]) -> None:
    """Write field elements as raw 32-byte big-endian values."""
    with open(path, "wb") as f:
        for v in values:
            f.write(_to_bytes(field(v)))


def write_config(path: Path, degree: int, size: int, challenge: int) -> None:
    """Write configuration header: degree(u32), size(u32), challenge(256-bit)."""
    with open(path, "wb") as f:
        f.write(_to_u32(degree))
        f.write(_to_u32(size))
        f.write(_to_bytes(field(challenge)))


# ---------------------------------------------------------------------------
# Test case generators
# ---------------------------------------------------------------------------

def make_case_a(challenge: int = 5) -> tuple[tuple[MLETable, ...], int]:
    """
    Case A: monomial x1*x2*x3, size=8 tables.
    Returns (mles, challenge).
    Per SPEC Section 4 ordering: [000, 100, 010, 110, 001, 101, 011, 111]
    X1 fastest-varying (LSB): 0,0,0,0,1,1,1,1
    X2 middle:                 0,0,1,1,0,0,1,1
    X3 MSB:                    0,1,0,1,0,1,0,1
    """
    x1 = MLETable((0, 0, 0, 0, 1, 1, 1, 1))
    x2 = MLETable((0, 0, 1, 1, 0, 0, 1, 1))
    x3 = MLETable((0, 1, 0, 1, 0, 1, 0, 1))
    return (x1, x2, x3), challenge


def make_case_b() -> list[tuple[tuple[MLETable, ...], int, str]]:
    """
    Case B: x1*x2 + x2*x3 as two independent terms.
    Returns list of (mles_term, challenge, label).
    """
    x1 = MLETable((0, 0, 0, 0, 1, 1, 1, 1))
    x2 = MLETable((0, 0, 1, 1, 0, 0, 1, 1))
    x3 = MLETable((0, 1, 0, 1, 0, 1, 0, 1))
    return [
        ((x1, x2), 5, "x1*x2"),
        ((x2, x3), 5, "x2*x3"),
    ]


def make_case_c(challenge: int = 5) -> tuple[tuple[MLETable, ...], int]:
    """
    Case C: Figure 1 style degree-4 term."""
    a = MLETable((1, 2, 3, 4, 5, 6, 7, 8))
    b = MLETable((2, 3, 4, 5, 6, 7, 8, 9))
    c = MLETable((1, 1, 2, 2, 3, 3, 4, 4))
    e = MLETable((3, 3, 3, 3, 5, 5, 5, 5))
    return (a, b, c, e), challenge


def random_term(degree: int, table_size: int, seed: int = 42) -> tuple[MLETable, ...]:
    rng = random.Random(seed)
    return tuple(
        MLETable(tuple(field(rng.getrandbits(256)) for _ in range(table_size)))
        for _ in range(degree)
    )


# ---------------------------------------------------------------------------
# Main generation
# ---------------------------------------------------------------------------

def generate_test_case(
    out_dir: Path,
    label: str,
    mles: Sequence[MLETable],
    challenge: int,
) -> None:
    """Generate binary test vectors for one SumCheck round."""
    case_dir = out_dir / label
    case_dir.mkdir(parents=True, exist_ok=True)

    degree, size = validate_term(mles)

    # Compute golden outputs
    samples = compute_round_evaluations(mles)
    updated = update_tables(mles, challenge)

    # Serialize inputs
    # tables.bin: interleaved [table0[0], table0[1], ..., table1[0], ...]
    tables_flat: list[int] = []
    for mle in mles:
        tables_flat.extend(mle.values)
    write_field_elems(case_dir / "tables.bin", tables_flat)
    write_config(case_dir / "config.bin", degree, size, challenge)

    # Serialize expected outputs
    write_field_elems(case_dir / "samples_exp.bin", samples)
    updated_flat: list[int] = []
    for mle in updated:
        updated_flat.extend(mle.values)
    write_field_elems(case_dir / "updated_exp.bin", updated_flat)

    # Write human-readable summary
    with open(case_dir / "summary.txt", "w") as f:
        f.write(f"Test case: {label}\n")
        f.write(f"Degree: {degree}, Table size: {size}, Challenge: {challenge}\n")
        f.write(f"Tables:\n")
        for idx, mle in enumerate(mles):
            f.write(f"  MLE {idx}: {list(mle.values)}\n")
        f.write(f"Round samples s(0..{degree}): {list(samples)}\n")
        f.write(f"Updated tables:\n")
        for idx, mle in enumerate(updated):
            f.write(f"  MLE {idx}: {list(mle.values)}\n")

    print(f"  Generated {case_dir}/")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", "-o",
        default="data",
        help="Output directory for test vectors (default: hls/data)",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Generate all regression test cases",
    )
    parser.add_argument(
        "--case",
        choices=["monomial", "biterm", "figure1", "random"],
        default=None,
        help="Generate a specific test case",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Random seed for random test cases",
    )
    args = parser.parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(parents=True, exist_ok=True)

    print("zkPHIRE HLS Test Vector Generator")
    print(f"Output directory: {out_dir}")
    print(f"Field prime: {FIELD_P}")
    print()

    # By default, generate all deterministic cases if no specific case given
    generate_all = args.all or args.case is None

    if generate_all or args.case == "monomial":
        print("--- Case A: Monomial x1*x2*x3 ---")
        mles, r = make_case_a(5)
        generate_test_case(out_dir, "case_a_r5", mles, r)
        mles, r = make_case_a(0)
        generate_test_case(out_dir, "case_a_r0", mles, r)
        mles, r = make_case_a(1)
        generate_test_case(out_dir, "case_a_r1", mles, r)

    if generate_all or args.case == "biterm":
        print("--- Case B: Two-term x1*x2 + x2*x3 ---")
        for mles, r, label in make_case_b():
            generate_test_case(out_dir, f"case_b_{label}", mles, r)

    if generate_all or args.case == "figure1":
        print("--- Case C: Figure 1 style degree-4 ---")
        mles, r = make_case_c(5)
        generate_test_case(out_dir, "case_c_r5", mles, r)

    if generate_all or args.case == "random":
        print("--- Randomized test cases ---")
        rng = random.Random(args.seed)
        for case_idx in range(8):
            degree = 2 + (case_idx % 4)  # degrees 2-5
            table_size = 1 << (2 + (case_idx % 3))  # sizes 4, 8, 16
            mles = random_term(degree, table_size, seed=args.seed + case_idx)
            challenge = field(rng.getrandbits(256))
            generate_test_case(out_dir, f"random_d{degree}_s{table_size}_c{case_idx}",
                               mles, challenge)

    print()
    print("Done. Run C-simulation with:")
    print("  vitis_hls -f run_hls.tcl")
    print()
    print("Or manually:")
    print("  cd hls && vitis_hls -f run_hls.tcl")


if __name__ == "__main__":
    main()
