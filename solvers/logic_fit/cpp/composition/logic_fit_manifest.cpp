#include "composition/logic_fit_manifest.hpp"

logic_fit_manifest::logic_fit_manifest(
    size_t arity,
    const std::vector<poly_sample>& samples,
    const poly_limits& limits,
    size_t frontier_capacity)
    : kernel_(limits)
    , rule_set_(kernel_, arity, samples)
    , search_(rule_set_.rules_, kernel_.sizes_, rule_set_.initial_, frontier_capacity) {}
