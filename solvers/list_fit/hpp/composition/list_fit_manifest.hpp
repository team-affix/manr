#ifndef LIST_FIT_MANIFEST_HPP
#define LIST_FIT_MANIFEST_HPP

#include <cstddef>
#include <vector>
#include "composition/list_fit_rules_manifest.hpp"
#include "composition/poly_kernel_manifest.hpp"
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_size.hpp"
#include "value_objects/list_sample.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_limits.hpp"

// Fits list programs to data under best-first search.
struct list_fit_manifest {
    using rule_set_t = list_fit_rules_manifest;
    struct search_t : best_first_search<rule_set_t::rules_t, expr_size> {
        using best_first_search<rule_set_t::rules_t, expr_size>::best_first_search;
    };

    list_fit_manifest(const std::vector<list_sample>& samples,
                      const poly_limits& limits,
                      const poly_fit_limits& fit_limits,
                      size_t frontier_capacity);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    search_t search_;
};

#endif
