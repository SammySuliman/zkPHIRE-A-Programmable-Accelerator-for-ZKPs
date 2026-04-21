#pragma once

#include "types.hpp"
#include "update_unit.hpp"

namespace zkphire {

inline void extend_pair(
    field_t f0,
    field_t f1,
    config_t degree,
    field_t extensions[ZKPHIRE_MAX_DEGREE + 1]
) {
#pragma HLS INLINE
    for (config_t x = 0; x <= ZKPHIRE_MAX_DEGREE; ++x) {
#pragma HLS UNROLL
        extensions[x] = (x <= degree) ? line_eval(f0, f1, field_from_u32(x)) : field_zero();
    }
}

}  // namespace zkphire
