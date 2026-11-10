#ifndef POLY_BASELINE_MANIFEST_HPP
#define POLY_BASELINE_MANIFEST_HPP

#include <cstddef>
#include <string>
#include "composition/poly_kernel_manifest.hpp"
#include "composition/poly_local_rules_manifest.hpp"
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_size.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_limits.hpp"

// Baseline: low-level rules only, under best-first search. The problem is infix polynomial text.
struct poly_baseline_manifest {
    using rule_set_t = poly_local_rules_manifest;
    struct search_t : best_first_search<rule_set_t::rules_t, expr_size> {
        using best_first_search<rule_set_t::rules_t, expr_size>::best_first_search;
    };

    poly_baseline_manifest(const std::string& problem, const poly_limits& limits, size_t frontier_capacity);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    const expr* initial_;
    search_t search_;
};

#endif
