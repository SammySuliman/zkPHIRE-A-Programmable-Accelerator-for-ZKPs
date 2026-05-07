#ifndef FIELD_ARITHMETIC_HPP
#define FIELD_ARITHMETIC_HPP

#include "types.hpp"

static const bn254_t BN254_P =
    bn254_t("0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47");
static const ap_uint<512> BN254_P_512 =
    ap_uint<512>("0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47");

inline bn254_t bn254_add(bn254_t a, bn254_t b) {
  bn254_t sum = a + b;
  if (sum >= BN254_P) {
    sum -= BN254_P;
  }
  return sum;
}

inline bn254_t bn254_sub(bn254_t a, bn254_t b) {
  if (a >= b) {
    return a - b;
  }
  return (a + BN254_P) - b;
}

inline bn254_t bn254_mul(bn254_t a, bn254_t b) {
  ap_uint<512> product = ap_uint<512>(a) * ap_uint<512>(b);
  ap_uint<512> reduced = product % BN254_P_512;
  return bn254_t(reduced);
}

#endif  // FIELD_ARITHMETIC_HPP
