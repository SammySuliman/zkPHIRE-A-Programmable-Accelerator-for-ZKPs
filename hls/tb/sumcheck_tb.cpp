#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "sumcheck_top.hpp"

using zkphire::ZKPHIRE_MAX_DEGREE;
using zkphire::ZKPHIRE_MAX_PAIR_COUNT;
using zkphire::ZKPHIRE_MAX_TABLE_SIZE;
using zkphire::axis_stream_t;
using zkphire::field_t;

namespace {

field_t tables[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_TABLE_SIZE];
field_t updated[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_PAIR_COUNT];
field_t samples[ZKPHIRE_MAX_DEGREE + 1];

field_t value(std::uint64_t v) {
    return field_t(v);
}

void clear_buffers() {
    for (unsigned factor = 0; factor < ZKPHIRE_MAX_DEGREE; ++factor) {
        for (unsigned row = 0; row < ZKPHIRE_MAX_TABLE_SIZE; ++row) {
            tables[factor][row] = 0;
        }
        for (unsigned row = 0; row < ZKPHIRE_MAX_PAIR_COUNT; ++row) {
            updated[factor][row] = 0;
        }
    }
    for (unsigned idx = 0; idx <= ZKPHIRE_MAX_DEGREE; ++idx) {
        samples[idx] = 0;
    }
}

void expect_equal(const std::string &label, field_t got, field_t expected) {
    if (got != expected) {
        std::cerr << "FAIL " << label << ": got " << zkphire::field_to_string(got)
                  << ", expected " << zkphire::field_to_string(expected) << "\n";
        std::exit(1);
    }
}

void load_rows(const std::vector<std::vector<std::uint64_t>> &rows) {
    clear_buffers();
    for (unsigned factor = 0; factor < rows.size(); ++factor) {
        for (unsigned row = 0; row < rows[factor].size(); ++row) {
            tables[factor][row] = value(rows[factor][row]);
        }
    }
}

void copy_updated_to_tables(unsigned degree, unsigned table_size) {
    field_t next_tables[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_PAIR_COUNT] = {};
    const unsigned next_size = table_size >> 1;
    for (unsigned factor = 0; factor < degree; ++factor) {
        for (unsigned row = 0; row < next_size; ++row) {
            next_tables[factor][row] = updated[factor][row];
        }
    }
    clear_buffers();
    for (unsigned factor = 0; factor < degree; ++factor) {
        for (unsigned row = 0; row < next_size; ++row) {
            tables[factor][row] = next_tables[factor][row];
        }
    }
}

void run_array_round(
    const std::string &label,
    unsigned degree,
    unsigned table_size,
    std::uint64_t challenge,
    const std::vector<field_t> &expected_samples
) {
    const std::uint32_t status = sumcheck_round_array(
        tables,
        value(challenge),
        degree,
        table_size,
        samples,
        updated
    );
    expect_equal(label + " status", status, zkphire::STATUS_OK);
    for (unsigned idx = 0; idx < expected_samples.size(); ++idx) {
        expect_equal(label + " sample[" + std::to_string(idx) + "]", samples[idx], expected_samples[idx]);
    }
}

void test_case_a_monomial_chain() {
    load_rows({
        {0, 1, 0, 1, 0, 1, 0, 1},
        {0, 0, 1, 1, 0, 0, 1, 1},
        {0, 0, 0, 0, 1, 1, 1, 1},
    });

    run_array_round("case A round 1", 3, 8, 5, {value(0), value(1), value(2), value(3)});
    copy_updated_to_tables(3, 8);
    run_array_round("case A round 2", 3, 4, 7, {value(0), value(5), value(10), value(15)});
    copy_updated_to_tables(3, 4);
    run_array_round("case A round 3", 3, 2, 11, {value(0), value(35), value(70), value(105)});

    field_t final_value = field_t(1);
    for (unsigned factor = 0; factor < 3; ++factor) {
        final_value = zkphire::field_mul(final_value, updated[factor][0]);
    }
    expect_equal("case A final", final_value, value(385));
}

void test_case_b_two_term_chain() {
    const std::vector<std::vector<std::uint64_t>> x1x2 = {
        {0, 1, 0, 1, 0, 1, 0, 1},
        {0, 0, 1, 1, 0, 0, 1, 1},
    };
    const std::vector<std::vector<std::uint64_t>> x2x3 = {
        {0, 0, 1, 1, 0, 0, 1, 1},
        {0, 0, 0, 0, 1, 1, 1, 1},
    };

    field_t term_a[2][ZKPHIRE_MAX_TABLE_SIZE] = {};
    field_t term_b[2][ZKPHIRE_MAX_TABLE_SIZE] = {};
    field_t next_a[2][ZKPHIRE_MAX_PAIR_COUNT] = {};
    field_t next_b[2][ZKPHIRE_MAX_PAIR_COUNT] = {};
    const std::uint64_t challenges[3] = {5, 7, 11};
    const std::uint64_t expected[3][3] = {
        {1, 3, 5},
        {0, 11, 22},
        {35, 42, 49},
    };

    for (unsigned factor = 0; factor < 2; ++factor) {
        for (unsigned row = 0; row < 8; ++row) {
            term_a[factor][row] = value(x1x2[factor][row]);
            term_b[factor][row] = value(x2x3[factor][row]);
        }
    }

    unsigned table_size = 8;
    for (unsigned round = 0; round < 3; ++round) {
        field_t samples_a[ZKPHIRE_MAX_DEGREE + 1] = {};
        field_t samples_b[ZKPHIRE_MAX_DEGREE + 1] = {};
        field_t full_samples[ZKPHIRE_MAX_DEGREE + 1] = {};

        field_t array_a[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_TABLE_SIZE] = {};
        field_t array_b[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_TABLE_SIZE] = {};
        field_t updates_a[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_PAIR_COUNT] = {};
        field_t updates_b[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_PAIR_COUNT] = {};
        for (unsigned factor = 0; factor < 2; ++factor) {
            for (unsigned row = 0; row < table_size; ++row) {
                array_a[factor][row] = term_a[factor][row];
                array_b[factor][row] = term_b[factor][row];
            }
        }

        expect_equal("case B term A status", sumcheck_round_array(array_a, value(challenges[round]), 2, table_size, samples_a, updates_a), zkphire::STATUS_OK);
        expect_equal("case B term B status", sumcheck_round_array(array_b, value(challenges[round]), 2, table_size, samples_b, updates_b), zkphire::STATUS_OK);
        for (unsigned idx = 0; idx < 3; ++idx) {
            full_samples[idx] = zkphire::field_add(samples_a[idx], samples_b[idx]);
            expect_equal("case B round " + std::to_string(round + 1) + " sample[" + std::to_string(idx) + "]", full_samples[idx], value(expected[round][idx]));
        }

        const unsigned next_size = table_size >> 1;
        for (unsigned factor = 0; factor < 2; ++factor) {
            for (unsigned row = 0; row < next_size; ++row) {
                next_a[factor][row] = updates_a[factor][row];
                next_b[factor][row] = updates_b[factor][row];
            }
        }
        for (unsigned factor = 0; factor < 2; ++factor) {
            for (unsigned row = 0; row < next_size; ++row) {
                term_a[factor][row] = next_a[factor][row];
                term_b[factor][row] = next_b[factor][row];
            }
        }
        table_size = next_size;
    }

    const field_t final_value = zkphire::field_add(
        zkphire::field_mul(term_a[0][0], term_a[1][0]),
        zkphire::field_mul(term_b[0][0], term_b[1][0])
    );
    expect_equal("case B final", final_value, value(112));
}

void test_case_c_figure_style() {
    load_rows({
        {1, 2, 3, 4, 5, 6, 7, 8},
        {2, 3, 4, 5, 6, 7, 8, 9},
        {1, 1, 2, 2, 3, 3, 4, 4},
        {3, 3, 3, 3, 5, 5, 5, 5},
    });
    run_array_round("case C", 4, 8, 5, {value(1648), value(2208), value(2856), value(3592), value(4416)});

    const std::uint64_t expected_updates[4][4] = {
        {6, 8, 10, 12},
        {7, 9, 11, 13},
        {1, 2, 3, 4},
        {3, 3, 5, 5},
    };
    for (unsigned factor = 0; factor < 4; ++factor) {
        for (unsigned row = 0; row < 4; ++row) {
            expect_equal("case C update", updated[factor][row], value(expected_updates[factor][row]));
        }
    }
}

void test_axis_wrapper() {
    axis_stream_t pairs_in;
    axis_stream_t samples_out;
    axis_stream_t updates_out;

    const std::vector<std::vector<std::uint64_t>> rows = {
        {1, 2, 3, 4},
        {5, 6, 7, 8},
    };
    for (unsigned pair_idx = 0; pair_idx < 2; ++pair_idx) {
        for (unsigned factor = 0; factor < 2; ++factor) {
            const bool final_word = pair_idx == 1 && factor == 1;
            pairs_in.write(zkphire::make_axis_word(value(rows[factor][2 * pair_idx]), false));
            pairs_in.write(zkphire::make_axis_word(value(rows[factor][2 * pair_idx + 1]), final_word));
        }
    }

    expect_equal("axis status", sumcheck_round_axis(pairs_in, samples_out, updates_out, value(3), 2, 4), zkphire::STATUS_OK);

    const std::uint64_t expected_samples[3] = {26, 44, 66};
    for (unsigned idx = 0; idx < 3; ++idx) {
        const auto word = samples_out.read();
        expect_equal("axis sample", zkphire::axis_data(word), value(expected_samples[idx]));
        if ((idx == 2) != bool(word.last)) {
            std::cerr << "FAIL axis sample TLAST at index " << idx << "\n";
            std::exit(1);
        }
    }

    const std::uint64_t expected_updates[4] = {4, 8, 6, 10};
    for (unsigned idx = 0; idx < 4; ++idx) {
        const auto word = updates_out.read();
        expect_equal("axis update", zkphire::axis_data(word), value(expected_updates[idx]));
        if ((idx == 3) != bool(word.last)) {
            std::cerr << "FAIL axis update TLAST at index " << idx << "\n";
            std::exit(1);
        }
    }
}

void test_modular_wraparound() {
    clear_buffers();
    const field_t p = zkphire::field_modulus();
    tables[0][0] = p - 1;
    tables[0][1] = 2;
    tables[1][0] = 7;
    tables[1][1] = p - 5;

    run_array_round("wraparound", 2, 2, 2, {p - 7, p - 10, p - 85});
    expect_equal("wraparound update 0", updated[0][0], value(5));
    expect_equal("wraparound update 1", updated[1][0], p - 17);
}

}  // namespace

int main() {
    test_case_a_monomial_chain();
    test_case_b_two_term_chain();
    test_case_c_figure_style();
    test_axis_wrapper();
    test_modular_wraparound();
    std::cout << "PASS HLS sumcheck testbench\n";
    return 0;
}
