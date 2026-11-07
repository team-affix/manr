#ifndef POLY_FIT_MANIFEST_HPP
#define POLY_FIT_MANIFEST_HPP

#include <cstddef>
#include <string>
#include <vector>
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/poly_fit_rules_manifest.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/poly_sample.hpp"

// Fits polynomial data under best-first search. `variables` names the inputs in the order each sample lists them.
struct poly_fit_manifest {
    using rule_set_t = poly_fit_rules_manifest;
    struct search_t : best_first_search<rule_set_t::rules_t, expr_size> {
        using best_first_search<rule_set_t::rules_t, expr_size>::best_first_search;
    };

    poly_fit_manifest(const std::vector<std::string>& variables,
                      const std::vector<poly_sample>& samples,
                      const poly_limits& limits,
                      const poly_fit_limits& fit_limits,
                      size_t frontier_capacity);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    search_t search_;
};

#endif
