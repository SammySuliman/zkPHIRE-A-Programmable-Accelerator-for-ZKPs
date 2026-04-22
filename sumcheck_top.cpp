#include "types.hpp"
#include "field_arithmetic.hpp"
#include "update_unit.hpp"
#include "extension_engine.hpp"
#include "product_lane.hpp"
#include "accumulator.hpp"

void sumcheck_top(bn254_t mle_inputs[MAX_DEGREE][INITIAL_TABLE_SIZE],
                  int degree,
                  bn254_t r,
                  bn254_t round_samples[MAX_SAMPLES],
                  bn254_t next_mle_tables[MAX_DEGREE][INITIAL_TABLE_SIZE / 2]) {
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=degree bundle=control
#pragma HLS INTERFACE s_axilite port=r bundle=control
#pragma HLS INTERFACE m_axi port=mle_inputs offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=round_samples offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=next_mle_tables offset=slave bundle=gmem2

  int bounded_degree = degree;
  if (bounded_degree < 0) {
    bounded_degree = 0;
  } else if (bounded_degree > MAX_DEGREE) {
    bounded_degree = MAX_DEGREE;
  }

  for (int x = 0; x < MAX_SAMPLES; ++x) {
#pragma HLS UNROLL
    round_samples[x] = 0;
  }

  bn254_t extended_factors[MAX_DEGREE][MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=extended_factors complete dim=0
  bn254_t current_products[MAX_SAMPLES];
#pragma HLS ARRAY_PARTITION variable=current_products complete

  for (int k = 0; k < (INITIAL_TABLE_SIZE / 2); ++k) {
#pragma HLS PIPELINE II=1
    for (int j = 0; j < MAX_DEGREE; ++j) {
#pragma HLS UNROLL
      if (j < bounded_degree) {
        extension_engine(mle_inputs[j][2 * k],
                         mle_inputs[j][2 * k + 1],
                         bounded_degree,
                         extended_factors[j]);
        next_mle_tables[j][k] =
            update_unit(mle_inputs[j][2 * k], mle_inputs[j][2 * k + 1], r);
      } else {
        next_mle_tables[j][k] = 0;
        for (int x = 0; x < MAX_SAMPLES; ++x) {
#pragma HLS UNROLL
          extended_factors[j][x] = 1;
        }
      }
    }

    product_lane(extended_factors, bounded_degree, current_products);
    accumulator(current_products, round_samples, bounded_degree);
  }
}
