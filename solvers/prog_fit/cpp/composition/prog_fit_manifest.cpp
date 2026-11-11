#include "composition/prog_fit_manifest.hpp"

prog_fit_manifest::prog_fit_manifest(const std::vector<prog_sample>& samples,
                                     const poly_limits& limits,
                                     size_t frontier_capacity)
    : kernel_(limits)
    , rule_set_(kernel_, samples)
    , search_(rule_set_.rules_, kernel_.sizes_, rule_set_.initial_, frontier_capacity) {}
