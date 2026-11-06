#ifndef POLY_MONOMIAL_HPP
#define POLY_MONOMIAL_HPP

#include <compare>
#include <vector>
#include "value_objects/poly_power.hpp"

// A product of variable powers, sorted by variable id with no zero exponents. Empty is the constant monomial 1.
struct poly_monomial {
    std::vector<poly_power> powers;
    auto operator<=>(const poly_monomial&) const = default;
};

#endif
