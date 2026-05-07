# Archive — Original Flat-Layout Implementation

These files are the initial flat-layout HLS implementation preserved for reference.
They were reorganized into the `hls/` and `hls_rfsoc/` directory structures.

- **HLS source**: `accumulator.hpp`, `extension_engine.hpp`, `field_arithmetic.hpp`,
  `product_lane.hpp`, `types.hpp`, `update_unit.hpp`, `sumcheck_top.cpp`
- **Testbench**: `sumcheck_tb.cpp`
- **Vector generation / test scripts**: `generate_vectors.py`, `run_tests.py`, `golden_sumcheck.py`
- **Test vectors**: `vec_expected_next_tables.txt`, `vec_expected_samples.txt`, `vec_inputs.txt`

For the current codebase, see `hls/`, `hls_rfsoc/`, and `docs/`.
