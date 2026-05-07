#ifndef PRODUCT_LANE_HPP
#define PRODUCT_LANE_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

inline void product_lane(bn254_t factors[MAX_DEGREE][MAX_SAMPLES],
                         int degree,
                         bn254_t products[MAX_SAMPLES]) {
#pragma HLS ARRAY_PARTITION variable=factors complete dim=0

  int bounded_degree = degree;
  if (bounded_degree < 0) {
    bounded_degree = 0;
  } else if (bounded_degree > MAX_DEGREE) {
    bounded_degree = MAX_DEGREE;
  }

  for (int x = 0; x < MAX_SAMPLES; ++x) {
#pragma HLS PIPELINE II=1
#pragma HLS UNROLL
    if (x <= bounded_degree) {
      bn254_t acc = 1;
      for (int j = 0; j < MAX_DEGREE; ++j) {
#pragma HLS UNROLL
        if (j < bounded_degree) {
          acc = bn254_mul(acc, factors[j][x]);
        }
      }
      products[x] = acc;
    } else {
      products[x] = 0;
    }
  }
}

#endif  // PRODUCT_LANE_HPP
