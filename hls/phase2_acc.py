#!/usr/bin/env python3
"""
Phase 2 Multi-Term Accumulation Verification

Demonstrates that the one-round SumCheck core, invoked once per term
with samples summed externally, correctly implements multi-term
compositions like x1*x2 + x2*x3.

This matches SPEC Section 8 Case B and the Phase 2 pass criteria:
  "x1*x2 + x2*x3 matches the deterministic regression exactly"

Usage:
  python3 phase2_acc.py
"""

from __future__ import annotations

import sys
from pathlib import Path

# Import the golden model
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from golden_sumcheck import (
    FIELD_P,
    field,
    mod_add,
    mod_mul,
    mod_sub,
    MLETable,
    compute_round_evaluations,
    update_tables,
    sumcheck_round,
    claim_from_tables,
    evaluate_round_at,
    assert_round_invariants,
    assert_protocol_chain,
)


def run_case_b_full_chain() -> None:
    """
    Case B: x1*x2 + x2*x3 with challenges (5, 7, 11)

    SPEC Section 8 expected round behavior:
      s1(t) = 1 + 2t  -> samples [1, 3, 5]
      s2(t) = 11t     -> samples [0, 11, 22]
      s3(t) = 35 + 7t -> samples [35, 42, 49]
      final value = 112
    """
    print("=== Phase 2: Multi-Term Accumulation ===")
    print(f"Field prime: {FIELD_P}")
    print()

    # MLE tables for x1, x2, x3
    x1 = MLETable.from_values([0, 0, 0, 0, 1, 1, 1, 1])
    x2 = MLETable.from_values([0, 0, 1, 1, 0, 0, 1, 1])
    x3 = MLETable.from_values([0, 1, 0, 1, 0, 1, 0, 1])

    challenges = (5, 7, 11)

    print("MLE tables (size 8):")
    print(f"  x1 = {list(x1.values)}")
    print(f"  x2 = {list(x2.values)}")
    print(f"  x3 = {list(x3.values)}")
    print()

    # --- Multi-term verification ---
    # Term 1: x1*x2, Term 2: x2*x3
    term1_tables = (x1, x2)
    term2_tables = (x2, x3)

    passed = True

    for round_idx, challenge in enumerate(challenges, start=1):
        # Process each term independently (as the HLS core does)
        result1 = sumcheck_round(term1_tables, challenge)
        result2 = sumcheck_round(term2_tables, challenge)

        # Phase 2 accumulation: sum per-term samples
        degree = 2  # max degree across both terms
        combined_samples = tuple(
            mod_add(s1, s2)
            for s1, s2 in zip(result1.evaluations, result2.evaluations)
        )

        # The updated tables carry forward independently per term
        term1_tables = result1.updated_tables
        term2_tables = result2.updated_tables

        # Verify round invariants on the combined samples
        # s(0) + s(1) should equal claim_before (sum of both terms)
        claim_before = mod_add(
            claim_from_tables(term1_tables if round_idx == 1 else term1_tables_pre),
            claim_from_tables(term2_tables if round_idx == 1 else term2_tables_pre),
        )

        # Wait, I need the claim_before from the *previous* round
        # In round 1, claim_before = sum of term1_claim + term2_claim
        if round_idx == 1:
            claim_before_r1 = mod_add(
                claim_from_tables((x1, x2)),
                claim_from_tables((x2, x3)),
            )
        else:
            claim_before_r1 = mod_add(
                claim_from_tables(term1_tables_pre),
                claim_from_tables(term2_tables_pre),
            )

        s0_plus_s1 = mod_add(combined_samples[0], combined_samples[1])

        inv1_ok = s0_plus_s1 == claim_before_r1
        if not inv1_ok:
            print(f"FAIL Round {round_idx}: s(0)+s(1) = {s0_plus_s1}, claim = {claim_before_r1}")
            passed = False

        print(f"Round {round_idx} (r={challenge}):")
        print(f"  Combined samples s(0..{degree}): {list(combined_samples)}")
        print(f"  s(0)+s(1) = {s0_plus_s1}, claim = {claim_before_r1} {'✓' if inv1_ok else '✗'}")
        print(f"  Term1 updated: x1={list(result1.updated_tables[0].values)}")
        print(f"                 x2={list(result1.updated_tables[1].values)}")
        print(f"  Term2 updated: x2={list(result2.updated_tables[0].values)}")
        print(f"                 x3={list(result2.updated_tables[1].values)}")

        # Save for next round's claim check
        term1_tables_pre = result1.updated_tables
        term2_tables_pre = result2.updated_tables

    # --- Final scalar check ---
    # After all rounds, each term collapses to a scalar
    # Term 1: x1_final * x2_final
    # Term 2: x2_final * x3_final
    t1_final = mod_mul(
        term1_tables_pre[0].values[0],
        term1_tables_pre[1].values[0],
    )
    t2_final = mod_mul(
        term2_tables_pre[0].values[0],
        term2_tables_pre[1].values[0],
    )
    final = mod_add(t1_final, t2_final)
    expected_final = 112  # From SPEC Section 8

    final_ok = final == expected_final
    if not final_ok:
        print(f"FAIL final scalar: got {final}, expected {expected_final}")
        passed = False

    print()
    print(f"Final scalar = {final}, expected {expected_final} {'✓' if final_ok else '✗'}")

    # --- Check against SPEC expected per-round values ---
    spec_expected = [
        (1, [1, 3, 5]),
        (2, [0, 11, 22]),
        (3, [35, 42, 49]),
    ]

    # Re-run to check individual round samples
    t1 = (x1, x2)
    t2 = (x2, x3)

    print()
    print("SPEC expected per-round samples:")
    for round_num, expected in spec_expected:
        r1 = sumcheck_round(t1, challenges[round_num - 1])
        r2 = sumcheck_round(t2, challenges[round_num - 1])
        combined = tuple(mod_add(s1, s2) for s1, s2 in zip(r1.evaluations, r2.evaluations))
        t1 = r1.updated_tables
        t2 = r2.updated_tables
        match = combined == tuple(expected)
        print(f"  Round {round_num}: got {list(combined)}, expected {expected} {'✓' if match else '✗'}")
        if not match:
            passed = False

    print()
    if passed:
        print("PASS: Phase 2 multi-term accumulation verified")
    else:
        print("FAIL: Phase 2 multi-term accumulation has discrepancies")
        sys.exit(1)


if __name__ == "__main__":
    run_case_b_full_chain()
