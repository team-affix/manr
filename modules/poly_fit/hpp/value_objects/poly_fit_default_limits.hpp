#ifndef POLY_FIT_DEFAULT_LIMITS_HPP
#define POLY_FIT_DEFAULT_LIMITS_HPP

#include "infrastructure/poly_interpolation_hypotheses.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "value_objects/poly_fit_limits.hpp"

// The data detectors' own default limits, gathered for callers that configure a whole fit solver at once.
constexpr poly_fit_limits k_poly_fit_default_limits{
    .max_degree = k_poly_fit_default_max_degree,
    .max_unknowns = k_poly_fit_default_max_unknowns,
    .max_starts = k_poly_fit_default_max_starts,
};

#endif
