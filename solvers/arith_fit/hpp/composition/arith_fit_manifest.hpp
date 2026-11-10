#ifndef ARITH_FIT_MANIFEST_HPP
#define ARITH_FIT_MANIFEST_HPP

#include <cstddef>
#include <vector>
#include "composition/arith_fit_rules_manifest.hpp"
#include "composition/arith_kernel_manifest.hpp"
#include "infrastructure/best_first_search.hpp"
#include "infrastructure/expr_size.hpp"
#include "value_objects/arith_sample.hpp"

// Fits integer samples with the smallest arithmetic expression best-first search finds.
struct arith_fit_manifest {
    using rule_set_t = arith_fit_rules_manifest;
    struct search_t : best_first_search<rule_set_t::rules_t, expr_size> {
        using best_first_search<rule_set_t::rules_t, expr_size>::best_first_search;
    };

    arith_fit_manifest(const std::vector<arith_sample>& samples, size_t frontier_capacity);

    arith_kernel_manifest kernel_;
    rule_set_t rule_set_;
    search_t search_;
};

#endif
