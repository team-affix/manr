#ifndef LOGIC_FIT_MANIFEST_HPP
#define LOGIC_FIT_MANIFEST_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/logic_fit_rules_manifest.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/poly_sample.hpp"

// Fits boolean functions to truth-table data under best-first search. `arity` is the number of inputs each sample
// lists.
struct logic_fit_manifest {
    using rule_set_t = logic_fit_rules_manifest;
    struct search_t : best_first_search<rule_set_t::rules_t, expr_size> {
        using best_first_search<rule_set_t::rules_t, expr_size>::best_first_search;
    };

    logic_fit_manifest(size_t arity,
                       const std::vector<poly_sample>& samples,
                       const poly_limits& limits,
                       size_t frontier_capacity);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    search_t search_;
};

#endif
