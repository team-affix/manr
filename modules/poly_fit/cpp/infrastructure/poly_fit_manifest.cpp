#include "infrastructure/poly_fit_manifest.hpp"

poly_fit_manifest::poly_fit_manifest(
    size_t arity,
    const std::vector<poly_sample>& samples,
    const poly_limits& limits,
    const poly_fit_limits& fit_limits,
    size_t frontier_capacity)
    : kernel_(limits)
    , rule_set_(kernel_, arity, samples, fit_limits)
    , search_(rule_set_.rules_, kernel_.sizes_, rule_set_.initial_, frontier_capacity) {}
