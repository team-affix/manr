#ifndef ARITH_SYMBOLS_HPP
#define ARITH_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the integer arithmetic vocabulary. `input` is the nullary functor standing for the model's input,
// which is distinct from extension terms. Integer literals are nullary functors named by their decimal text.
struct arith_symbols {
    uint32_t input;
    uint32_t add;
    uint32_t sub;
    uint32_t mul;
    auto operator<=>(const arith_symbols&) const = default;
};

#endif
