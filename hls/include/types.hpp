#pragma once

#include <cstdint>
#ifndef __SYNTHESIS__
#include <string>
#endif

#if !defined(ZKPHIRE_USE_BOOST_TYPES) && __has_include(<ap_int.h>)
#include <ap_int.h>
#define ZKPHIRE_HAS_VITIS_TYPES 1
#define ZKPHIRE_HAS_BOOST_TYPES 0
#define ZKPHIRE_HAS_SOFTWARE64_TYPES 0
#elif __has_include(<boost/multiprecision/cpp_int.hpp>)
#include <boost/multiprecision/cpp_int.hpp>
#define ZKPHIRE_HAS_VITIS_TYPES 0
#define ZKPHIRE_HAS_BOOST_TYPES 1
#define ZKPHIRE_HAS_SOFTWARE64_TYPES 0
#else
#define ZKPHIRE_HAS_VITIS_TYPES 0
#define ZKPHIRE_HAS_BOOST_TYPES 0
#define ZKPHIRE_HAS_SOFTWARE64_TYPES 1
#endif

namespace zkphire {

static constexpr unsigned ZKPHIRE_MAX_DEGREE = 6;
static constexpr unsigned ZKPHIRE_MAX_TABLE_SIZE = 1024;
static constexpr unsigned ZKPHIRE_MAX_PAIR_COUNT = ZKPHIRE_MAX_TABLE_SIZE / 2;

using config_t = std::uint32_t;

#if ZKPHIRE_HAS_VITIS_TYPES
using field_t = ap_uint<256>;
using wide_field_t = ap_uint<512>;
#elif ZKPHIRE_HAS_BOOST_TYPES
using field_t = boost::multiprecision::uint256_t;
using wide_field_t = boost::multiprecision::uint512_t;
#else
using field_t = std::uint64_t;
using wide_field_t = unsigned __int128;
#endif

enum status_t : std::uint32_t {
    STATUS_OK = 0,
    STATUS_BAD_DEGREE = 1,
    STATUS_BAD_TABLE_SIZE = 2,
};

inline field_t field_from_u64s(
    std::uint64_t limb0,
    std::uint64_t limb1,
    std::uint64_t limb2,
    std::uint64_t limb3
) {
#if ZKPHIRE_HAS_SOFTWARE64_TYPES
    (void)limb1;
    (void)limb2;
    (void)limb3;
    return limb0;
#elif ZKPHIRE_HAS_VITIS_TYPES
    field_t value = 0;
    value.range(63, 0) = limb0;
    value.range(127, 64) = limb1;
    value.range(191, 128) = limb2;
    value.range(255, 192) = limb3;
    return value;
#else
    field_t value = limb3;
    value <<= 64;
    value |= limb2;
    value <<= 64;
    value |= limb1;
    value <<= 64;
    value |= limb0;
    return value;
#endif
}

inline field_t field_modulus() {
#if ZKPHIRE_HAS_SOFTWARE64_TYPES
    // Dependency-free host fallback used only when neither Vitis nor Boost is
    // available. The synthesizable Vitis path above still uses BN254.
    return 0xffffffff00000001ULL;
#else
    return field_from_u64s(
        0x43e1f593f0000001ULL,
        0x2833e84879b97091ULL,
        0xb85045b68181585dULL,
        0x30644e72e131a029ULL
    );
#endif
}

inline field_t field_zero() {
    return field_t(0);
}

inline field_t field_one() {
    return field_t(1);
}

inline field_t field_from_u32(std::uint32_t value) {
    return field_t(value);
}

inline field_t field_add(field_t a, field_t b) {
    const wide_field_t p = wide_field_t(field_modulus());
    wide_field_t sum = wide_field_t(a) + wide_field_t(b);
    if (sum >= p) {
        sum -= p;
    }
    return field_t(sum);
}

inline field_t field_sub(field_t a, field_t b) {
    const field_t p = field_modulus();
    if (a >= b) {
        return field_t(a - b);
    }
    return field_t(wide_field_t(a) + wide_field_t(p) - wide_field_t(b));
}

inline field_t field_mul(field_t a, field_t b) {
    const wide_field_t p = wide_field_t(field_modulus());
    wide_field_t product = wide_field_t(a) * wide_field_t(b);
    wide_field_t reduced = product % p;
    return field_t(reduced);
}

inline field_t field_normalize(field_t value) {
    const wide_field_t p = wide_field_t(field_modulus());
    return field_t(wide_field_t(value) % p);
}

inline bool is_power_of_two(config_t value) {
    return value != 0 && ((value & (value - 1)) == 0);
}

inline status_t validate_round_config(config_t degree, config_t table_size) {
    if (degree == 0 || degree > ZKPHIRE_MAX_DEGREE) {
        return STATUS_BAD_DEGREE;
    }
    if (!is_power_of_two(table_size) || table_size < 2 || table_size > ZKPHIRE_MAX_TABLE_SIZE) {
        return STATUS_BAD_TABLE_SIZE;
    }
    return STATUS_OK;
}

#ifndef __SYNTHESIS__
inline std::string field_to_string(const field_t &value) {
#if ZKPHIRE_HAS_VITIS_TYPES
    return value.to_string(10);
#elif ZKPHIRE_HAS_BOOST_TYPES
    return value.convert_to<std::string>();
#else
    return std::to_string(value);
#endif
}
#endif

}  // namespace zkphire
