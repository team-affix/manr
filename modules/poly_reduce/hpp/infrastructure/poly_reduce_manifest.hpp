#ifndef POLY_REDUCE_MANIFEST_HPP
#define POLY_REDUCE_MANIFEST_HPP

#include <cstddef>
#include <string>
#include "infrastructure/expr_size.hpp"
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_rules_manifest.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_limits.hpp"

// Full rules (low-level + detectors) under best-first search. The problem is infix polynomial text.
struct poly_reduce_manifest {
    using rule_set_t = poly_rules_manifest;
    struct search_t : best_first_search<rule_set_t::rules_t, expr_size> {
        using best_first_search<rule_set_t::rules_t, expr_size>::best_first_search;
    };

    poly_reduce_manifest(const std::string& problem, const poly_limits& limits, size_t frontier_capacity);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    const expr* initial_;
    search_t search_;
};

#endif
