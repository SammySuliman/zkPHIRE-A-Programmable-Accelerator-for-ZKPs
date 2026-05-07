#ifndef TYPES_HPP
#define TYPES_HPP

#include <ap_int.h>

typedef ap_uint<256> bn254_t;

const int MAX_DEGREE = 6;
const int MAX_SAMPLES = MAX_DEGREE + 1;
const int INITIAL_TABLE_SIZE = 8;
const int TABLE_SIZE = 8;

#endif  // TYPES_HPP
