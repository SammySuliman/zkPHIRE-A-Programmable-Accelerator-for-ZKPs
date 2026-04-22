#ifndef EXTENSION_ENGINE_HPP
#define EXTENSION_ENGINE_HPP

#include "types.hpp"
#include "update_unit.hpp"

const int MAX_SAMPLES = MAX_DEGREE + 1;

inline void extension_engine(bn254_t f0,
                             bn254_t f1,
                             int term_degree,
                             bn254_t results[MAX_SAMPLES]) {
  int bounded_degree = term_degree;
  if (bounded_degree < 0) {
    bounded_degree = 0;
  } else if (bounded_degree > MAX_DEGREE) {
    bounded_degree = MAX_DEGREE;
  }

  for (int i = 0; i < MAX_SAMPLES; ++i) {
#pragma HLS UNROLL
    if (i <= bounded_degree) {
      results[i] = update_unit(f0, f1, bn254_t(i));
    } else {
      results[i] = 0;
    }
  }
}

#endif  // EXTENSION_ENGINE_HPP
