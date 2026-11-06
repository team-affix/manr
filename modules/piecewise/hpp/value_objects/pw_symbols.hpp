#ifndef PW_SYMBOLS_HPP
#define PW_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the piecewise vocabulary: `pw(case(key, value), ..., otherwise)`.
struct pw_symbols {
    uint32_t piecewise;
    uint32_t branch;
    auto operator<=>(const pw_symbols&) const = default;
};

#endif
