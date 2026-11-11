#ifndef RAT_FRACTION_HPP
#define RAT_FRACTION_HPP

#include "value_objects/poly_polynomial.hpp"

// A rational function as one quotient of polynomials, `numerator / denominator`.
struct rat_fraction {
    poly_polynomial numerator;
    poly_polynomial denominator;
    auto operator<=>(const rat_fraction&) const = default;
};

#endif
