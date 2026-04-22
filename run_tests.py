import unittest

from golden_sumcheck import (
    FIELD_P,
    MLETable,  # Importing your custom class!
    compute_round_evaluations,
    update_tables
)

# Hardware Accumulator equivalent: summing samples across multiple terms
def add_samples(s1, s2):
    return [(a + b) % FIELD_P for a, b in zip(s1, s2)]

class TestZkPhireGoldenModel(unittest.TestCase):
    def setUp(self):
        # We wrap the standard lists in your MLETable class
        self.x1_table = MLETable([0, 1, 0, 1, 0, 1, 0, 1])
        self.x2_table = MLETable([0, 0, 1, 1, 0, 0, 1, 1])
        self.x3_table = MLETable([0, 0, 0, 0, 1, 1, 1, 1])

    def test_case_a_monomial(self):
        """ Case A: f(x1, x2, x3) = x1 * x2 * x3 """
        term_tables = [self.x1_table, self.x2_table, self.x3_table]
        
        # --- Round 1 ---
        # Notice we pass term_tables directly, NOT wrapped in another list!
        samples_r1 = compute_round_evaluations(term_tables) 
        self.assertEqual(list(samples_r1), [0, 1, 2, 3], "Round 1 mismatch")
        term_tables = update_tables(term_tables, 5)

        # --- Round 2 ---
        samples_r2 = compute_round_evaluations(term_tables)
        self.assertEqual(list(samples_r2), [0, 5, 10, 15], "Round 2 mismatch")
        term_tables = update_tables(term_tables, 7)

        # --- Round 3 ---
        samples_r3 = compute_round_evaluations(term_tables)
        self.assertEqual(list(samples_r3), [0, 35, 70, 105], "Round 3 mismatch")
        term_tables = update_tables(term_tables, 11)
        
        final_val = (term_tables[0][0] * term_tables[1][0] * term_tables[2][0]) % FIELD_P
        self.assertEqual(final_val, 385, "Final evaluation mismatch")

    def test_case_b_two_term_composition(self):
        """ Case B: f(x1, x2, x3) = x1*x2 + x2*x3 """
        # We define each term separately
        term1 = [self.x1_table, self.x2_table]
        term2 = [self.x2_table, self.x3_table]
        
        # --- Round 1 ---
        s1_r1 = compute_round_evaluations(term1)
        s2_r1 = compute_round_evaluations(term2)
        samples_r1 = add_samples(s1_r1, s2_r1) # Accumulate the terms
        self.assertEqual(samples_r1, [1, 3, 5])
        
        term1 = update_tables(term1, 5)
        term2 = update_tables(term2, 5)

        # --- Round 2 ---
        s1_r2 = compute_round_evaluations(term1)
        s2_r2 = compute_round_evaluations(term2)
        samples_r2 = add_samples(s1_r2, s2_r2)
        self.assertEqual(samples_r2, [0, 11, 22])
        
        term1 = update_tables(term1, 7)
        term2 = update_tables(term2, 7)

        # --- Round 3 ---
        s1_r3 = compute_round_evaluations(term1)
        s2_r3 = compute_round_evaluations(term2)
        samples_r3 = add_samples(s1_r3, s2_r3)
        self.assertEqual(samples_r3, [35, 42, 49])
        
        term1 = update_tables(term1, 11)
        term2 = update_tables(term2, 11)
        
        t1_final = (term1[0][0] * term1[1][0]) % FIELD_P
        t2_final = (term2[0][0] * term2[1][0]) % FIELD_P
        self.assertEqual((t1_final + t2_final) % FIELD_P, 112)

    def test_invariant_halving(self):
        table_size = len(self.x1_table)
        updated = update_tables([self.x1_table], 5)
        self.assertEqual(len(updated[0]), table_size // 2)

if __name__ == '__main__':
    unittest.main(verbosity=2)