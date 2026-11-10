#ifndef POLY_FIT_LIMITS_HPP
#define POLY_FIT_LIMITS_HPP

#include <compare>
#include <cstddef>

// The limits the polynomial data detectors observe, supplied from the outermost level (a solver manifest's
// constructor). Defaults: `k_poly_fit_default_limits`.
struct poly_fit_limits {
    size_t max_degree;      // highest total degree the interpolation and split hypotheses try
    size_t max_unknowns;    // most coefficients (monomials) the interpolator solves for
    size_t max_starts;      // starting points per degree (and per piece), so a fit can begin past an outlier
    auto operator<=>(const poly_fit_limits&) const = default;
};

#endif
