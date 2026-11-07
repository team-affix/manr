#ifndef POLY_INTERPOLANT_HPP
#define POLY_INTERPOLANT_HPP

#include <compare>
#include <cstddef>
#include "value_objects/poly_polynomial.hpp"

// A polynomial interpolated through data, and how many coefficients it was solved for: a fit explains the data only
// when it reproduces more points than that.
struct poly_interpolant {
    poly_polynomial polynomial;
    size_t unknowns;
    auto operator<=>(const poly_interpolant&) const = default;
};

#endif
