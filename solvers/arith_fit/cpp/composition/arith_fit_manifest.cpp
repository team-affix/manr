#include "composition/arith_fit_manifest.hpp"

arith_fit_manifest::arith_fit_manifest(const std::vector<arith_sample>& samples, size_t frontier_capacity)
    : kernel_()
    , rule_set_(kernel_, samples)
    , search_(rule_set_.rules_, kernel_.sizes_, rule_set_.initial_, frontier_capacity) {}
