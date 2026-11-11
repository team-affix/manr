#ifndef PROG_FIT_MANIFEST_HPP
#define PROG_FIT_MANIFEST_HPP

#include <cstddef>
#include <vector>
#include "composition/poly_kernel_manifest.hpp"
#include "composition/prog_fit_rules_manifest.hpp"
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_size.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/prog_sample.hpp"

// Fits generic programs to samples of any shape under best-first search.
struct prog_fit_manifest {
    using rule_set_t = prog_fit_rules_manifest;
    struct search_t : best_first_search<rule_set_t::rules_t, expr_size> {
        using best_first_search<rule_set_t::rules_t, expr_size>::best_first_search;
    };

    prog_fit_manifest(const std::vector<prog_sample>& samples, const poly_limits& limits, size_t frontier_capacity);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    search_t search_;
};

#endif
