#ifndef POLY_LOCAL_RULES_MANIFEST_HPP
#define POLY_LOCAL_RULES_MANIFEST_HPP

#include "composition/poly_kernel_manifest.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "rules/poly_local_rules.hpp"

// The baseline rule set: only the low-level local rules, applied at every position. No detectors.
struct poly_local_rules_manifest {
    using kernel_t = poly_kernel_manifest;

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct low_level_t : poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::poly_local_rules;
    };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        low_level_t>;
    struct local_rules_t : local_rules_base_t {
        using local_rules_base_t::local_rules_base_t;
    };
    struct root_rules_t : proposer_union<> {
        using proposer_union<>::proposer_union;
    };
    struct subterms_t : subterm_proposer<local_rules_t, expr_pool> {
        using subterm_proposer<local_rules_t, expr_pool>::subterm_proposer;
    };
    struct rules_t : proposer_union<subterms_t, root_rules_t> {
        using proposer_union<subterms_t, root_rules_t>::proposer_union;
    };

    poly_local_rules_manifest(kernel_t& kernel);

    // rules
    low_level_t low_level_;
    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
};

#endif
