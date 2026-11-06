#ifndef POLY_SYMBOLS_HPP
#define POLY_SYMBOLS_HPP

#include <compare>
#include <cstdint>

// Functor ids of the polynomial vocabulary. `pow`'s exponent is a non-negative integer literal. Integer literals are
// nullary functors named by their decimal text; every other nullary functor is a variable.
struct poly_symbols {
    uint32_t add;
    uint32_t sub;
    uint32_t mul;
    uint32_t pow;
    auto operator<=>(const poly_symbols&) const = default;
};

#endif
