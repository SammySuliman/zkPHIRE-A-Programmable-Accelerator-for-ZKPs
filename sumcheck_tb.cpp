#include <iostream>
#include <fstream>
#include <string>
#include "types.hpp"

// Declare the top-level hardware function
void sumcheck_top(bn254_t mle_inputs[MAX_DEGREE][INITIAL_TABLE_SIZE],
                  int degree,
                  bn254_t r,
                  bn254_t round_samples[MAX_SAMPLES],
                  bn254_t next_mle_tables[MAX_DEGREE][INITIAL_TABLE_SIZE / 2]);

// Helper function to read a line and convert to bn254_t
bool read_next_hex(std::ifstream& file, bn254_t& val) {
    std::string line;
    if (std::getline(file, line)) {
        // ap_uint constructor handles the "0x" prefix automatically if base is set to 16
        val = bn254_t(line.c_str(), 16);
        return true;
    }
    return false;
}

int main() {
    std::cout << "========================================" << std::endl;
    case "Starting zkPHIRE SumCheck HLS Testbench..." << std::endl;
    std::cout << "========================================" << std::endl;

    // ---------------------------------------------------------
    // 1. Initialize Hardware Inputs
    // ---------------------------------------------------------
    bn254_t mle_inputs[MAX_DEGREE][INITIAL_TABLE_SIZE] = {0};
    int test_degree = 3;      // Case A is a 3-variable monomial
    bn254_t challenge_r = 5;  // Hardcoded in generate_vectors.py
    
    bn254_t hw_round_samples[MAX_SAMPLES] = {0};
    bn254_t hw_next_tables[MAX_DEGREE][INITIAL_TABLE_SIZE / 2] = {0};

    // ---------------------------------------------------------
    // 2. Load Input Vectors
    // ---------------------------------------------------------
    std::ifstream f_inputs("vec_inputs.txt");
    if (!f_inputs.is_open()) {
        std::cerr << "Error: Could not open vec_inputs.txt" << std::endl;
        return 1;
    }

    // Python flattens: table0, then table1, then table2
    for (int i = 0; i < test_degree; ++i) {
        for (int j = 0; j < INITIAL_TABLE_SIZE; ++j) {
            read_next_hex(f_inputs, mle_inputs[i][j]);
        }
    }
    f_inputs.close();

    // ---------------------------------------------------------
    // 3. Load Expected Output Vectors
    // ---------------------------------------------------------
    bn254_t expected_samples[MAX_SAMPLES] = {0};
    std::ifstream f_samples("vec_expected_samples.txt");
    if (!f_samples.is_open()) {
        std::cerr << "Error: Could not open vec_expected_samples.txt" << std::endl;
        return 1;
    }
    
    // We expect (test_degree + 1) samples for the round polynomial
    for (int i = 0; i <= test_degree; ++i) {
        read_next_hex(f_samples, expected_samples[i]);
    }
    f_samples.close();

    bn254_t expected_next_tables[MAX_DEGREE][INITIAL_TABLE_SIZE / 2] = {0};
    std::ifstream f_next("vec_expected_next_tables.txt");
    if (!f_next.is_open()) {
        std::cerr << "Error: Could not open vec_expected_next_tables.txt" << std::endl;
        return 1;
    }

    // Expected updated tables (halved size)
    for (int i = 0; i < test_degree; ++i) {
        for (int j = 0; j < (INITIAL_TABLE_SIZE / 2); ++j) {
            read_next_hex(f_next, expected_next_tables[i][j]);
        }
    }
    f_next.close();

    // ---------------------------------------------------------
    // 4. Execute the Hardware Datapath
    // ---------------------------------------------------------
    std::cout << "Running sumcheck_top hardware module..." << std::endl;
    sumcheck_top(mle_inputs, test_degree, challenge_r, hw_round_samples, hw_next_tables);

    // ---------------------------------------------------------
    // 5. Verification
    // ---------------------------------------------------------
    int errors = 0;

    // Verify Round Samples
    std::cout << "\n--- Verifying Round Samples ---" << std::endl;
    for (int i = 0; i <= test_degree; ++i) {
        if (hw_round_samples[i] != expected_samples[i]) {
            std::cerr << "[FAIL] Sample " << i << " Mismatch!" << std::endl;
            std::cerr << "       Expected: " << expected_samples[i].to_string(16) << std::endl;
            std::cerr << "       Got:      " << hw_round_samples[i].to_string(16) << std::endl;
            errors++;
        } else {
            std::cout << "[PASS] Sample " << i << ": " << hw_round_samples[i].to_string(16) << std::endl;
        }
    }

    // Verify Updated Tables
    std::cout << "\n--- Verifying Updated MLE Tables ---" << std::endl;
    for (int i = 0; i < test_degree; ++i) {
        for (int j = 0; j < (INITIAL_TABLE_SIZE / 2); ++j) {
            if (hw_next_tables[i][j] != expected_next_tables[i][j]) {
                std::cerr << "[FAIL] Table " << i << " Index " << j << " Mismatch!" << std::endl;
                std::cerr << "       Expected: " << expected_next_tables[i][j].to_string(16) << std::endl;
                std::cerr << "       Got:      " << hw_next_tables[i][j].to_string(16) << std::endl;
                errors++;
            }
        }
        if (errors == 0) {
            std::cout << "[PASS] Table " << i << " updated correctly." << std::endl;
        }
    }

    // ---------------------------------------------------------
    // 6. Conclusion
    // ---------------------------------------------------------
    std::cout << "\n========================================" << std::endl;
    if (errors == 0) {
        std::cout << "SUCCESS: All HLS outputs match the Python Golden Model!" << std::endl;
    } else {
        std::cerr << "FAILURE: Found " << errors << " errors during verification." << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return errors;
}