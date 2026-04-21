#pragma once

#include "types.hpp"

namespace zkphire {

inline void reset_accumulators(field_t accumulators[ZKPHIRE_MAX_DEGREE + 1]) {
#pragma HLS INLINE
    for (config_t x = 0; x <= ZKPHIRE_MAX_DEGREE; ++x) {
#pragma HLS UNROLL
        accumulators[x] = field_zero();
    }
}

inline void accumulate_products(
    config_t degree,
    const field_t products[ZKPHIRE_MAX_DEGREE + 1],
    field_t accumulators[ZKPHIRE_MAX_DEGREE + 1]
) {
#pragma HLS INLINE
    for (config_t x = 0; x <= ZKPHIRE_MAX_DEGREE; ++x) {
#pragma HLS UNROLL
        if (x <= degree) {
            accumulators[x] = field_add(accumulators[x], products[x]);
        }
    }
}

}  // namespace zkphire
