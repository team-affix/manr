#ifndef RAT_SYMBOLS_HPP
#define RAT_SYMBOLS_HPP

#include <cstdint>

// The vocabulary of rational functions: the quotient `div(a, b)`, written `a / b` in polynomial text, which has no
// value where b is 0.
struct rat_symbols {
    uint32_t quotient;
    auto operator<=>(const rat_symbols&) const = default;
};

#endif
