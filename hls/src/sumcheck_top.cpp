#include "sumcheck_top.hpp"

#include "accumulator.hpp"
#include "extension_engine.hpp"
#include "product_lane.hpp"
#include "update_unit.hpp"

using zkphire::ZKPHIRE_MAX_DEGREE;
using zkphire::ZKPHIRE_MAX_PAIR_COUNT;
using zkphire::ZKPHIRE_MAX_TABLE_SIZE;
using zkphire::axis_stream_t;
using zkphire::axis_word_t;
using zkphire::config_t;
using zkphire::field_t;

namespace {

void compute_pair_contribution(
    field_t f0,
    field_t f1,
    config_t degree,
    field_t products[ZKPHIRE_MAX_DEGREE + 1]
) {
#pragma HLS INLINE
    field_t extensions[ZKPHIRE_MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=extensions complete
    zkphire::extend_pair(f0, f1, degree, extensions);
    zkphire::multiply_lane_extensions(degree, extensions, products);
}

}  // namespace

std::uint32_t sumcheck_round_array(
    field_t input_tables[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_TABLE_SIZE],
    field_t challenge,
    config_t degree,
    config_t table_size,
    field_t samples[ZKPHIRE_MAX_DEGREE + 1],
    field_t updated_tables[ZKPHIRE_MAX_DEGREE][ZKPHIRE_MAX_PAIR_COUNT]
) {
#pragma HLS INLINE off
    const zkphire::status_t status = zkphire::validate_round_config(degree, table_size);
    if (status != zkphire::STATUS_OK) {
        return status;
    }

    field_t accumulators[ZKPHIRE_MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=accumulators complete
    zkphire::reset_accumulators(accumulators);

    const config_t pair_count = table_size >> 1;
    for (config_t pair_idx = 0; pair_idx < pair_count; ++pair_idx) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=512
        field_t products[ZKPHIRE_MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=products complete
        zkphire::reset_lane_products(products);

        for (config_t factor_idx = 0; factor_idx < degree; ++factor_idx) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=6
            const field_t f0 = input_tables[factor_idx][2 * pair_idx];
            const field_t f1 = input_tables[factor_idx][2 * pair_idx + 1];
            compute_pair_contribution(f0, f1, degree, products);
            updated_tables[factor_idx][pair_idx] = zkphire::update_pair(f0, f1, challenge);
        }

        zkphire::accumulate_products(degree, products, accumulators);
    }

    for (config_t x = 0; x <= ZKPHIRE_MAX_DEGREE; ++x) {
#pragma HLS UNROLL
        samples[x] = (x <= degree) ? accumulators[x] : zkphire::field_zero();
    }

    return zkphire::STATUS_OK;
}

std::uint32_t sumcheck_round_axis(
    axis_stream_t &pairs_in,
    axis_stream_t &samples_out,
    axis_stream_t &updates_out,
    field_t challenge,
    config_t degree,
    config_t table_size
) {
#pragma HLS INTERFACE axis port=pairs_in
#pragma HLS INTERFACE axis port=samples_out
#pragma HLS INTERFACE axis port=updates_out
#pragma HLS INTERFACE s_axilite port=challenge bundle=control
#pragma HLS INTERFACE s_axilite port=degree bundle=control
#pragma HLS INTERFACE s_axilite port=table_size bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS DATAFLOW disable_start_propagation

    const zkphire::status_t status = zkphire::validate_round_config(degree, table_size);
    if (status != zkphire::STATUS_OK) {
        return status;
    }

    field_t accumulators[ZKPHIRE_MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=accumulators complete
    zkphire::reset_accumulators(accumulators);

    const config_t pair_count = table_size >> 1;
    const config_t total_updates = degree * pair_count;
    config_t update_count = 0;

    for (config_t pair_idx = 0; pair_idx < pair_count; ++pair_idx) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=512
        field_t products[ZKPHIRE_MAX_DEGREE + 1];
#pragma HLS ARRAY_PARTITION variable=products complete
        zkphire::reset_lane_products(products);

        for (config_t factor_idx = 0; factor_idx < degree; ++factor_idx) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=6
#pragma HLS PIPELINE II=1
            const axis_word_t f0_word = pairs_in.read();
            const axis_word_t f1_word = pairs_in.read();
            const field_t f0 = zkphire::axis_data(f0_word);
            const field_t f1 = zkphire::axis_data(f1_word);

            compute_pair_contribution(f0, f1, degree, products);

            const field_t updated = zkphire::update_pair(f0, f1, challenge);
            ++update_count;
            updates_out.write(zkphire::make_axis_word(updated, update_count == total_updates));
        }

        zkphire::accumulate_products(degree, products, accumulators);
    }

    for (config_t x = 0; x <= degree; ++x) {
#pragma HLS LOOP_TRIPCOUNT min=2 max=7
        samples_out.write(zkphire::make_axis_word(accumulators[x], x == degree));
    }

    return zkphire::STATUS_OK;
}
