#ifndef ACCUMULATOR_HPP
#define ACCUMULATOR_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

inline void accumulator(bn254_t current_products[MAX_SAMPLES],
                        bn254_t running_sums[MAX_SAMPLES],
                        int term_degree) {
#pragma HLS ARRAY_PARTITION variable=running_sums complete

  int bounded_degree = term_degree;
  if (bounded_degree < 0) {
    bounded_degree = 0;
  } else if (bounded_degree > MAX_DEGREE) {
    bounded_degree = MAX_DEGREE;
  }

  for (int x = 0; x <= bounded_degree; ++x) {
#pragma HLS PIPELINE
    running_sums[x] = bn254_add(running_sums[x], current_products[x]);
  }
}

#endif  // ACCUMULATOR_HPP
