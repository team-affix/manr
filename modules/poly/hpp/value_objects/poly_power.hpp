#ifndef POLY_POWER_HPP
#define POLY_POWER_HPP

#include <compare>
#include <cstdint>

// One variable of a monomial raised to a positive exponent. `variable` is the variable's functor id.
struct poly_power {
    uint32_t variable;
    uint32_t exponent;
    auto operator<=>(const poly_power&) const = default;
};

#endif
