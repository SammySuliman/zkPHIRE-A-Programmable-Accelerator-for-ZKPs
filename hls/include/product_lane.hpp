#pragma once

#include "types.hpp"

namespace zkphire {

inline void reset_lane_products(field_t products[ZKPHIRE_MAX_DEGREE + 1]) {
#pragma HLS INLINE
    for (config_t x = 0; x <= ZKPHIRE_MAX_DEGREE; ++x) {
#pragma HLS UNROLL
        products[x] = field_one();
    }
}

inline void multiply_lane_extensions(
    config_t degree,
    const field_t extensions[ZKPHIRE_MAX_DEGREE + 1],
    field_t products[ZKPHIRE_MAX_DEGREE + 1]
) {
#pragma HLS INLINE
    for (config_t x = 0; x <= ZKPHIRE_MAX_DEGREE; ++x) {
#pragma HLS UNROLL
        if (x <= degree) {
            products[x] = field_mul(products[x], extensions[x]);
        }
    }
}

}  // namespace zkphire
