#ifndef PW_SYMBOLS_HPP
#define PW_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the piecewise vocabulary: `pw(branch, ..., otherwise)`, where a branch is `case(key, value)` (applies
// at exactly the input `key`) or `when(condition, value)` (applies wherever the domain says `condition` holds). The
// first branch that applies gives the value.
struct pw_symbols {
    uint32_t piecewise;
    uint32_t branch;
    uint32_t region;
    auto operator<=>(const pw_symbols&) const = default;
};

#endif
