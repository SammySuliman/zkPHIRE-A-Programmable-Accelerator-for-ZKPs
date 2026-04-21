#pragma once

#include "types.hpp"

namespace zkphire {

inline field_t line_eval(field_t f0, field_t f1, field_t z) {
#pragma HLS INLINE
    return field_add(f0, field_mul(field_sub(f1, f0), z));
}

inline field_t update_pair(field_t f0, field_t f1, field_t challenge) {
#pragma HLS INLINE
    return line_eval(f0, f1, challenge);
}

}  // namespace zkphire
