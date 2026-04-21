#!/usr/bin/env python3
"""Generate JSON test vectors for the HLS one-round SumCheck core."""

from __future__ import annotations

import argparse
import json
import random
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))

from golden_sumcheck import (  # noqa: E402
    MLETable,
    compute_round_evaluations,
    random_term,
    update_tables,
)


def serialize_case(name: str, mles: tuple[MLETable, ...], challenge: int) -> dict:
    return {
        "name": name,
        "degree": len(mles),
        "table_size": len(mles[0]),
        "challenge": challenge,
        "input_tables": [list(mle.values) for mle in mles],
        "expected_samples": list(compute_round_evaluations(mles)),
        "expected_updated_tables": [list(mle.values) for mle in update_tables(mles, challenge)],
    }


def deterministic_cases() -> list[dict]:
    x1 = MLETable.from_values([0, 1, 0, 1, 0, 1, 0, 1])
    x2 = MLETable.from_values([0, 0, 1, 1, 0, 0, 1, 1])
    x3 = MLETable.from_values([0, 0, 0, 0, 1, 1, 1, 1])
    figure = (
        MLETable.from_values([1, 2, 3, 4, 5, 6, 7, 8]),
        MLETable.from_values([2, 3, 4, 5, 6, 7, 8, 9]),
        MLETable.from_values([1, 1, 2, 2, 3, 3, 4, 4]),
        MLETable.from_values([3, 3, 3, 3, 5, 5, 5, 5]),
    )
    return [
        serialize_case("monomial_x1_x2_x3_round1", (x1, x2, x3), 5),
        serialize_case("two_term_part_x1_x2_round1", (x1, x2), 5),
        serialize_case("two_term_part_x2_x3_round1", (x2, x3), 5),
        serialize_case("figure1_degree4_round", figure, 5),
    ]


def random_cases(count: int, seed: int) -> list[dict]:
    rng = random.Random(seed)
    cases = []
    for idx in range(count):
        degree = 2 + (idx % 5)
        table_size = 1 << (1 + (idx % 4))
        term = random_term(rng, degree=degree, table_size=table_size)
        challenge = rng.randrange(0, 97)
        cases.append(serialize_case(f"random_deg{degree}_case{idx}", term, challenge))
    return cases


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--random-cases", type=int, default=4)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--pretty", action="store_true")
    args = parser.parse_args()

    payload = {
        "format": "zkphire-hls-vectors-v1",
        "cases": deterministic_cases() + random_cases(args.random_cases, args.seed),
    }
    print(json.dumps(payload, indent=2 if args.pretty else None))


if __name__ == "__main__":
    main()
