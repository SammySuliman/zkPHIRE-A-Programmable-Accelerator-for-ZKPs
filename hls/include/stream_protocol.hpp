#pragma once

#include "types.hpp"

#if ZKPHIRE_HAS_VITIS_TYPES && __has_include(<hls_stream.h>) && __has_include(<ap_axi_sdata.h>)
#include <ap_axi_sdata.h>
#include <hls_stream.h>
#define ZKPHIRE_HAS_VITIS_STREAMS 1
#else
#include <queue>
#define ZKPHIRE_HAS_VITIS_STREAMS 0
namespace hls {
template <typename T>
class stream {
  public:
    bool empty() const { return values_.empty(); }
    void write(const T &value) { values_.push(value); }
    T read() {
        T value = values_.front();
        values_.pop();
        return value;
    }

  private:
    std::queue<T> values_;
};
}  // namespace hls
#endif

namespace zkphire {

#if ZKPHIRE_HAS_VITIS_STREAMS
using axis_word_t = ap_axiu<256, 0, 0, 0>;
#else
struct axis_word_t {
    field_t data;
    bool last;
};
#endif

using axis_stream_t = hls::stream<axis_word_t>;

inline axis_word_t make_axis_word(field_t data, bool last) {
    axis_word_t word;
    word.data = data;
    word.last = last;
    return word;
}

inline field_t axis_data(const axis_word_t &word) {
    return field_t(word.data);
}

}  // namespace zkphire
