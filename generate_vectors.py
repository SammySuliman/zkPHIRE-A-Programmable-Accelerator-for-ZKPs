from golden_sumcheck import MLETable, compute_round_evaluations, update_tables

def save_hex_vector(filename, data_list):
    with open(filename, 'w') as f:
        for val in data_list:
            # Save as hex. Vitis HLS ap_uint easily parses standard "0x..." strings
            f.write(f"{hex(val)}\n")

# Recreate Case A Round 1 setup
term_tables = [
    MLETable([0, 1, 0, 1, 0, 1, 0, 1]), # x1
    MLETable([0, 0, 1, 1, 0, 0, 1, 1]), # x2
    MLETable([0, 0, 0, 0, 1, 1, 1, 1])  # x3
]

challenge_r = 5

# 1. Save the Initial Tables (Flattened into one long list)
flat_inputs = [val for table in term_tables for val in table]
save_hex_vector("vec_inputs.txt", flat_inputs)

# 2. Calculate and Save Expected Samples
samples_r1 = compute_round_evaluations(term_tables)
save_hex_vector("vec_expected_samples.txt", samples_r1)

# 3. Calculate and Save the Updated Tables (Flattened)
updated_tables = update_tables(term_tables, challenge_r)
flat_next = [val for table in updated_tables for val in table]
save_hex_vector("vec_expected_next_tables.txt", flat_next)

print("Test vectors generated successfully!")