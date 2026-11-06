#ifndef ARITH_SAMPLE_HPP
#define ARITH_SAMPLE_HPP

#include <compare>
#include <cstdint>

struct arith_sample {
    int64_t input;
    int64_t output;
    auto operator<=>(const arith_sample&) const = default;
};

#endif
