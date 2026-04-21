#pragma once

#include "stream_protocol.hpp"
#include "types.hpp"

std::uint32_t sumcheck_round_array(
    zkphire::field_t input_tables[zkphire::ZKPHIRE_MAX_DEGREE][zkphire::ZKPHIRE_MAX_TABLE_SIZE],
    zkphire::field_t challenge,
    zkphire::config_t degree,
    zkphire::config_t table_size,
    zkphire::field_t samples[zkphire::ZKPHIRE_MAX_DEGREE + 1],
    zkphire::field_t updated_tables[zkphire::ZKPHIRE_MAX_DEGREE][zkphire::ZKPHIRE_MAX_PAIR_COUNT]
);

std::uint32_t sumcheck_round_axis(
    zkphire::axis_stream_t &pairs_in,
    zkphire::axis_stream_t &samples_out,
    zkphire::axis_stream_t &updates_out,
    zkphire::field_t challenge,
    zkphire::config_t degree,
    zkphire::config_t table_size
);
