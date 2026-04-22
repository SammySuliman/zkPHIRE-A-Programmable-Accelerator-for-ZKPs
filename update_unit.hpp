#ifndef UPDATE_UNIT_HPP
#define UPDATE_UNIT_HPP

#include "types.hpp"
#include "field_arithmetic.hpp"

inline bn254_t update_unit(bn254_t f0, bn254_t f1, bn254_t z) {
  // Reused for both Extension (z = 0..d) and Update (z = verifier challenge r).
  bn254_t diff = bn254_sub(f1, f0);
  bn254_t prod = bn254_mul(diff, z);
  return bn254_add(f0, prod);
}

#endif  // UPDATE_UNIT_HPP
