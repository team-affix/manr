#include "infrastructure/poly_baseline_manifest.hpp"

poly_baseline_manifest::poly_baseline_manifest(const std::string& problem, size_t frontier_capacity)
    : kernel_(256)
    , rule_set_(kernel_)
    , initial_(kernel_.parser_.parse(problem))
    , search_(rule_set_.rules_, kernel_.sizes_, initial_, frontier_capacity) {}
