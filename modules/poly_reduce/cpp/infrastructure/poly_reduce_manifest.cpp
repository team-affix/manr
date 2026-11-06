#include "infrastructure/poly_reduce_manifest.hpp"

poly_reduce_manifest::poly_reduce_manifest(const std::string& problem, size_t frontier_capacity)
    : kernel_(256)
    , rule_set_(kernel_)
    , initial_(kernel_.parser_.parse(problem))
    , search_(rule_set_.rules_, kernel_.sizes_, initial_, frontier_capacity) {}
