#ifndef POLY_DEFAULT_LIMITS_HPP
#define POLY_DEFAULT_LIMITS_HPP

#include "infrastructure/poly_arithmetic.hpp"
#include "infrastructure/poly_divisibility_filter.hpp"
#include "infrastructure/poly_expander.hpp"
#include "infrastructure/poly_factor_detector.hpp"
#include "infrastructure/poly_linear_factor_finder.hpp"
#include "infrastructure/poly_power_detector.hpp"
#include "infrastructure/poly_root_extractor.hpp"
#include "value_objects/poly_limits.hpp"

// Every rule's own default limits, gathered for callers that configure a whole solver at once. The values live with
// the rules (their constructors' default arguments); this only collects them.
constexpr poly_limits k_poly_default_limits{
    .max_terms = k_poly_default_max_terms,
    .max_exponent = k_poly_default_max_exponent,
    .max_power_degree = k_poly_default_max_power_degree,
    .max_root_starts = k_poly_default_max_root_starts,
    .max_factor_candidates = k_poly_default_max_factor_candidates,
    .divisibility_points = k_poly_default_divisibility_points,
    .max_constant_divisors = k_poly_default_max_constant_divisors,
    .max_constant_divisor = k_poly_default_max_constant_divisor,
    .max_linear_candidates = k_poly_default_max_linear_candidates,
    .max_root_divisor = k_poly_default_max_root_divisor,
    .linear_shift_points = k_poly_default_linear_shift_points,
    .max_shifted_terms = k_poly_default_max_terms,
};

#endif
