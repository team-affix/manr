#ifndef LOGIC_PARITY_HPP
#define LOGIC_PARITY_HPP

#include <compare>
#include <cstdint>

// An affine function over GF(2): the xor of the variables in `variables` (bit i is the i-th variable), negated when
// `inverted`.
struct logic_parity {
    uint64_t variables;
    bool inverted;
    auto operator<=>(const logic_parity&) const = default;
};

#endif
