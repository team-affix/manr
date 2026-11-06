#ifndef POLY_LOCAL_RULES_MANIFEST_HPP
#define POLY_LOCAL_RULES_MANIFEST_HPP

#include "infrastructure/expr_pool.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_local_rules.hpp"
#include "infrastructure/subterm_proposer.hpp"

// The baseline rule set: only the low-level local rules, applied at every position. No detectors.
struct poly_local_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    struct local_t : poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::poly_local_rules;
    };
    struct rules_t : subterm_proposer<local_t, expr_pool> {
        using subterm_proposer<local_t, expr_pool>::subterm_proposer;
    };

    poly_local_rules_manifest(kernel_t& kernel);

    local_t local_;
    rules_t rules_;
};

#endif
