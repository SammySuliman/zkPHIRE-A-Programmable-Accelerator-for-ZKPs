"""
zkPHIRE SumCheck golden model and verification harness.

This models one SumCheck round for a single product term
    f(X) = prod_j mle_j(X)
as described in zkPHIRE Section III.C and Figure 1.

The same affine rule is used for both:
1. MLE update at the verifier challenge r
2. Extension from Xi in {0, 1} out to Xi in {0, ..., d}

The self-tests focus on invariants the FPGA datapath must preserve:
1. Extension/product/sum matches a direct reference definition
2. s(0) + s(1) matches the original product-table claim
3. Updating every MLE with challenge r makes the next claim equal s(r)
4. Repeating rounds keeps the protocol chain consistent until tables collapse
"""

from __future__ import annotations

import argparse
import random
from dataclasses import dataclass
from typing import Iterable, Iterator, Sequence


# BN254 scalar field prime used in the spec.
FIELD_P = 21888242871839275222246405745257275088548364400416034343698204186575808495617


def mod_add(a: int, b: int) -> int:
    return (a + b) % FIELD_P


def mod_sub(a: int, b: int) -> int:
    return (a - b) % FIELD_P


def mod_mul(a: int, b: int) -> int:
    return (a * b) % FIELD_P


def mod_inv(a: int) -> int:
    if a % FIELD_P == 0:
        raise ZeroDivisionError("cannot invert zero in the field")
    return pow(a, FIELD_P - 2, FIELD_P)


def field(x: int) -> int:
    return x % FIELD_P


def affine_line_eval(f0: int, f1: int, point: int) -> int:
    """
    Shared affine rule from the paper:
        f(point) = f0 * (1 - point) + f1 * point
                 = f0 + (f1 - f0) * point
    """
    point = field(point)
    return mod_add(f0, mod_mul(mod_sub(f1, f0), point))


@dataclass(frozen=True)
class MLETable:
    values: tuple[int, ...]

    def __post_init__(self) -> None:
        if not self.values:
            raise ValueError("MLE table cannot be empty")
        if len(self.values) & (len(self.values) - 1):
            raise ValueError("MLE table size must be a power of two")
        object.__setattr__(self, "values", tuple(field(v) for v in self.values))

    @classmethod
    def from_values(cls, values: Iterable[int]) -> "MLETable":
        return cls(tuple(values))

    def __len__(self) -> int:
        return len(self.values)

    def __getitem__(self, idx: int) -> int:
        return self.values[idx]

    def pairs(self) -> Iterator[tuple[int, int]]:
        if len(self.values) == 1:
            raise ValueError("cannot form pairs from a size-1 MLE table")
        for idx in range(0, len(self.values), 2):
            yield self.values[idx], self.values[idx + 1]


@dataclass(frozen=True)
class RoundResult:
    evaluations: tuple[int, ...]
    updated_tables: tuple[MLETable, ...]


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


def extend_pair(f0: int, f1: int, degree: int) -> tuple[int, ...]:
    return tuple(affine_line_eval(f0, f1, x) for x in range(degree + 1))


def compute_round_evaluations(mles: Sequence[MLETable]) -> tuple[int, ...]:
    """
    Hardware-shaped reference:
    pair -> extensions -> termwise products -> accumulator over rows.
    """
    degree, size = validate_term(mles)
    if size < 2:
        raise ValueError("one SumCheck round requires tables of size at least 2")
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


def evaluate_round_at(mles: Sequence[MLETable], point: int) -> int:
    """
    Direct mathematical definition of the round polynomial at an arbitrary point.
    """
    degree, size = validate_term(mles)
    if size < 2:
        raise ValueError("one SumCheck round requires tables of size at least 2")
    total = 0

    for pair_idx in range(size // 2):
        product = 1
        for mle_idx in range(degree):
            mle = mles[mle_idx]
            f0 = mle[2 * pair_idx]
            f1 = mle[2 * pair_idx + 1]
            product = mod_mul(product, affine_line_eval(f0, f1, point))
        total = mod_add(total, product)

    return total


def compute_round_evaluations_reference(mles: Sequence[MLETable]) -> tuple[int, ...]:
    degree, _ = validate_term(mles)
    return tuple(evaluate_round_at(mles, x) for x in range(degree + 1))


def update_tables(mles: Sequence[MLETable], challenge: int) -> tuple[MLETable, ...]:
    _, size = validate_term(mles)
    if size < 2:
        raise ValueError("one SumCheck round requires tables of size at least 2")
    return tuple(
        MLETable.from_values(
            affine_line_eval(f0, f1, challenge) for f0, f1 in mle.pairs()
        )
        for mle in mles
    )


def sumcheck_round(mles: Sequence[MLETable], challenge: int) -> RoundResult:
    return RoundResult(
        evaluations=compute_round_evaluations(mles),
        updated_tables=update_tables(mles, challenge),
    )


def claim_from_tables(mles: Sequence[MLETable]) -> int:
    _, size = validate_term(mles)
    total = 0
    for row_idx in range(size):
        product = 1
        for mle in mles:
            product = mod_mul(product, mle[row_idx])
        total = mod_add(total, product)
    return total


def interpolate_from_samples(samples: Sequence[int], point: int) -> int:
    """
    Evaluate the degree-(n-1) polynomial defined by samples at x = 0..n-1.
    """
    if not samples:
        raise ValueError("cannot interpolate an empty sample set")
    if len(samples) == 1:
        return field(samples[0])

    point = field(point)
    total = 0
    for i, sample in enumerate(samples):
        numerator = 1
        denominator = 1
        for j in range(len(samples)):
            if i == j:
                continue
            numerator = mod_mul(numerator, mod_sub(point, j))
            denominator = mod_mul(denominator, mod_sub(i, j))
        total = mod_add(total, mod_mul(sample, mod_mul(numerator, mod_inv(denominator))))
    return total


def rounds_needed(table_size: int) -> int:
    if table_size <= 0 or table_size & (table_size - 1):
        raise ValueError("table size must be a positive power of two")
    return table_size.bit_length() - 1


def evaluate_mle_table(table: MLETable, challenges: Sequence[int]) -> int:
    current = table
    for challenge in challenges:
        if len(current) == 1:
            break
        current = update_tables((current,), challenge)[0]
    if len(current) != 1:
        raise ValueError(
            f"need exactly {rounds_needed(len(table))} challenges to fully evaluate table"
        )
    return current[0]


def evaluate_product_at_challenge_vector(
    mles: Sequence[MLETable], challenges: Sequence[int]
) -> int:
    result = 1
    for mle in mles:
        result = mod_mul(result, evaluate_mle_table(mle, challenges))
    return result


def assert_round_invariants(
    mles: Sequence[MLETable], challenge: int, *, label: str = "round"
) -> RoundResult:
    degree, size = validate_term(mles)
    if size < 2:
        raise AssertionError(f"{label}: cannot verify a round on size-1 tables")

    samples = compute_round_evaluations(mles)
    reference_samples = compute_round_evaluations_reference(mles)
    if samples != reference_samples:
        raise AssertionError(
            f"{label}: extension/product/sum path disagrees with direct definition"
        )

    claim_before = claim_from_tables(mles)
    if mod_add(samples[0], samples[1]) != claim_before:
        raise AssertionError(f"{label}: s(0) + s(1) does not match the round claim")

    updated_tables = update_tables(mles, challenge)
    if len(updated_tables[0]) != size // 2:
        raise AssertionError(f"{label}: table update did not halve the MLE size")

    claim_after = claim_from_tables(updated_tables)
    direct_at_challenge = evaluate_round_at(mles, challenge)
    interpolated_at_challenge = interpolate_from_samples(samples, challenge)

    if claim_after != direct_at_challenge:
        raise AssertionError(f"{label}: updated-table claim does not equal s(r)")
    if claim_after != interpolated_at_challenge:
        raise AssertionError(f"{label}: sampled polynomial does not interpolate to s(r)")

    normalized_challenge = field(challenge)
    if normalized_challenge == 0:
        for old, new in zip(mles, updated_tables):
            expected = tuple(old[idx] for idx in range(0, size, 2))
            if new.values != expected:
                raise AssertionError(f"{label}: r = 0 should select even-index entries")
    if normalized_challenge == 1:
        for old, new in zip(mles, updated_tables):
            expected = tuple(old[idx] for idx in range(1, size, 2))
            if new.values != expected:
                raise AssertionError(f"{label}: r = 1 should select odd-index entries")

    if len(samples) != degree + 1:
        raise AssertionError(f"{label}: expected exactly degree + 1 samples")

    return RoundResult(evaluations=samples, updated_tables=updated_tables)


def assert_protocol_chain(
    mles: Sequence[MLETable], challenges: Sequence[int], *, label: str
) -> None:
    if not challenges:
        raise ValueError("protocol-chain verification requires at least one challenge")

    expected_rounds = rounds_needed(len(mles[0]))
    if len(challenges) != expected_rounds:
        raise ValueError(
            f"{label}: need {expected_rounds} challenges for a full protocol chain"
        )

    current = tuple(mles)
    for round_idx, challenge in enumerate(challenges, start=1):
        result = assert_round_invariants(
            current,
            challenge,
            label=f"{label}/round{round_idx}",
        )
        current = result.updated_tables

    final_claim = 1
    for mle in current:
        if len(mle) != 1:
            raise AssertionError(f"{label}: tables did not collapse to scalars")
        final_claim = mod_mul(final_claim, mle[0])

    direct_final_claim = evaluate_product_at_challenge_vector(mles, challenges)
    if final_claim != direct_final_claim:
        raise AssertionError(f"{label}: final scalar claim is inconsistent")


def random_field_element(rng: random.Random) -> int:
    mode = rng.randrange(6)
    if mode == 0:
        return rng.randrange(0, 64)
    if mode == 1:
        return field(FIELD_P - 1 - rng.randrange(0, 64))
    return rng.getrandbits(256) % FIELD_P


def random_term(rng: random.Random, degree: int, table_size: int) -> tuple[MLETable, ...]:
    return tuple(
        MLETable.from_values(random_field_element(rng) for _ in range(table_size))
        for _ in range(degree)
    )


def make_figure1_term() -> tuple[MLETable, ...]:
    return (
        MLETable.from_values([1, 2, 3, 4, 5, 6, 7, 8]),
        MLETable.from_values([2, 3, 4, 5, 6, 7, 8, 9]),
        MLETable.from_values([1, 1, 2, 2, 3, 3, 4, 4]),
        MLETable.from_values([3, 3, 3, 3, 5, 5, 5, 5]),
    )


def make_boolean_variable_tables() -> tuple[MLETable, MLETable, MLETable]:
    return (
        MLETable.from_values([0, 1, 0, 1, 0, 1, 0, 1]),
        MLETable.from_values([0, 0, 1, 1, 0, 0, 1, 1]),
        MLETable.from_values([0, 0, 0, 0, 1, 1, 1, 1]),
    )


def assert_expected_round_samples(
    mles: Sequence[MLETable],
    challenges: Sequence[int],
    expected_rounds: Sequence[Sequence[int]],
    *,
    label: str,
) -> tuple[MLETable, ...]:
    current = tuple(mles)
    for round_idx, (challenge, expected) in enumerate(
        zip(challenges, expected_rounds), start=1
    ):
        result = assert_round_invariants(
            current,
            challenge,
            label=f"{label}/round{round_idx}",
        )
        if result.evaluations != tuple(field(v) for v in expected):
            raise AssertionError(
                f"{label}/round{round_idx}: samples {result.evaluations} "
                f"did not match expected {tuple(expected)}"
            )
        current = result.updated_tables
    return current


def assert_spec_regression_targets() -> None:
    x1, x2, x3 = make_boolean_variable_tables()
    challenges = (5, 7, 11)

    final_tables = assert_expected_round_samples(
        (x1, x2, x3),
        challenges,
        expected_rounds=(
            (0, 1, 2, 3),
            (0, 5, 10, 15),
            (0, 35, 70, 105),
        ),
        label="spec-case-a",
    )
    if claim_from_tables(final_tables) != 385:
        raise AssertionError("spec-case-a: final value did not match 385")

    term_a = (x1, x2)
    term_b = (x2, x3)
    expected_rounds = (
        (1, 3, 5),
        (0, 11, 22),
        (35, 42, 49),
    )
    for round_idx, (challenge, expected) in enumerate(
        zip(challenges, expected_rounds), start=1
    ):
        samples_a = compute_round_evaluations(term_a)
        samples_b = compute_round_evaluations(term_b)
        combined = tuple(mod_add(a, b) for a, b in zip(samples_a, samples_b))
        if combined != expected:
            raise AssertionError(
                f"spec-case-b/round{round_idx}: samples {combined} "
                f"did not match expected {expected}"
            )
        term_a = update_tables(term_a, challenge)
        term_b = update_tables(term_b, challenge)

    final_value = mod_add(claim_from_tables(term_a), claim_from_tables(term_b))
    if final_value != 112:
        raise AssertionError("spec-case-b: final value did not match 112")


def run_demo() -> None:
    term = make_figure1_term()
    challenge = 5
    result = sumcheck_round(term, challenge)

    print("=== zkPHIRE Golden Model Demo ===")
    print(f"Field prime p = {FIELD_P}")
    print(f"Degree = {len(term)}, table size = {len(term[0])}, challenge r = {challenge}")
    print()

    for name, mle in zip(("a", "b", "c", "e"), term):
        print(f"{name} = {list(mle.values)}")

    print()
    print(f"s(0..{len(term)}) = {list(result.evaluations)}")
    print(f"s(0) + s(1) = {mod_add(result.evaluations[0], result.evaluations[1])}")
    print(f"claim before update = {claim_from_tables(term)}")
    print()

    for name, mle in zip(("a'", "b'", "c'", "e'"), result.updated_tables):
        print(f"{name} = {list(mle.values)}")


def run_self_tests(seed: int, random_cases_per_degree: int) -> None:
    rng = random.Random(seed)

    deterministic_term = make_figure1_term()
    for challenge in (0, 1, 5, FIELD_P - 1):
        assert_round_invariants(
            deterministic_term,
            challenge,
            label=f"figure1-r={challenge}",
        )
    assert_protocol_chain(
        deterministic_term,
        challenges=(5, 7, 11),
        label="figure1-chain",
    )
    print("PASS deterministic Figure 1 style checks")

    assert_spec_regression_targets()
    print("PASS SPEC deterministic regression targets")

    wrap_term = (
        MLETable.from_values([FIELD_P - 1, 2, FIELD_P - 3, 4]),
        MLETable.from_values([7, FIELD_P - 5, 9, FIELD_P - 11]),
    )
    for challenge in (0, 1, 2, FIELD_P - 2):
        assert_round_invariants(
            wrap_term,
            challenge,
            label=f"wraparound-r={challenge}",
        )
    assert_protocol_chain(wrap_term, challenges=(3, FIELD_P - 7), label="wraparound-chain")
    print("PASS modular wraparound checks")

    for degree in range(2, 7):
        for case_idx in range(random_cases_per_degree):
            table_size = 1 << (1 + (case_idx % 4))
            term = random_term(rng, degree=degree, table_size=table_size)
            challenge = random_field_element(rng)
            assert_round_invariants(
                term,
                challenge,
                label=f"deg{degree}-case{case_idx}",
            )

            chain = tuple(
                random_field_element(rng) for _ in range(rounds_needed(table_size))
            )
            assert_protocol_chain(
                term,
                challenges=chain,
                label=f"deg{degree}-case{case_idx}-chain",
            )
        print(
            f"PASS degree {degree}: {random_cases_per_degree} random round checks "
            f"and protocol chains"
        )

    print("PASS: verified sumcheck round invariants for degrees 2-6")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--test",
        action="store_true",
        help="run deterministic and randomized verification logic",
    )
    parser.add_argument(
        "--demo",
        action="store_true",
        help="print a small Figure 1 style example",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=7,
        help="random seed for --test",
    )
    parser.add_argument(
        "--random-cases",
        type=int,
        default=12,
        help="random cases per degree for --test",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.demo or not args.test:
        run_demo()
    if args.test:
        run_self_tests(seed=args.seed, random_cases_per_degree=args.random_cases)


if __name__ == "__main__":
    main()
