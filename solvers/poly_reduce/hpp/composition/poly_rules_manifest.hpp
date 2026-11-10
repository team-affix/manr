#ifndef POLY_RULES_MANIFEST_HPP
#define POLY_RULES_MANIFEST_HPP

#include <cstdint>
#include <vector>
#include "composition/poly_kernel_manifest.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "rules/poly_collector.hpp"
#include "rules/poly_component_splitter.hpp"
#include "rules/poly_factor_detector.hpp"
#include "rules/poly_hole_binder.hpp"
#include "rules/poly_horner_detector.hpp"
#include "rules/poly_like_terms.hpp"
#include "rules/poly_local_rules.hpp"
#include "rules/poly_power_detector.hpp"

// The full polynomial rule set over a kernel: the low-level local rules and every detector, applied at every
// position, and the hole binder, applied to the whole expression (a binding substitutes everywhere). `rules_` is what a
// search injects; another solver that reduces polynomials borrows `local_rules_t` into its own local rules.
struct poly_rules_manifest {
    using kernel_t = poly_kernel_manifest;

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct low_level_t : poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::poly_local_rules;
    };
    using collector_base_t = poly_collector<kernel_t::expander_t, kernel_t::renderer_t, kernel_t::renderer_t, expr_size>;
    struct collector_t : collector_base_t {
        using collector_base_t::collector_base_t;
    };
    struct horner_t : poly_horner_detector<kernel_t::expander_t, kernel_t::horner_t, expr_size> {
        using poly_horner_detector<kernel_t::expander_t, kernel_t::horner_t, expr_size>::poly_horner_detector;
    };
    using factor_base_t = poly_factor_detector<kernel_t::expander_t, poly_divisibility_filter, poly_arithmetic,
                                               kernel_t::linear_factors_t, kernel_t::renderer_t, kernel_t::renderer_t,
                                               expr_pool, expr_size>;
    struct factor_t : factor_base_t {
        using factor_base_t::factor_base_t;
    };
    using power_base_t = poly_power_detector<kernel_t::expander_t, kernel_t::roots_t, kernel_t::renderer_t,
                                             kernel_t::renderer_t, expr_pool, kernel_t::literals_t, expr_size>;
    struct power_t : power_base_t {
        using power_base_t::power_base_t;
    };
    struct splitter_t
        : poly_component_splitter<kernel_t::expander_t, kernel_t::renderer_t, expr_pool, kernel_t::literals_t> {
        using poly_component_splitter<kernel_t::expander_t, kernel_t::renderer_t, expr_pool,
                                      kernel_t::literals_t>::poly_component_splitter;
    };
    struct like_terms_t
        : poly_like_terms<kernel_t::expander_t, kernel_t::literals_t, expr_pool, kernel_t::literals_t, expr_size> {
        using poly_like_terms<kernel_t::expander_t, kernel_t::literals_t, expr_pool, kernel_t::literals_t,
                              expr_size>::poly_like_terms;
    };
    using holes_base_t = poly_hole_binder<kernel_t::expander_t, kernel_t::renderer_t, kernel_t::renderer_t,
                                          poly_arithmetic, poly_arithmetic, poly_arithmetic, poly_arithmetic,
                                          kernel_t::roots_t, expr_pool, expr_pool, expr_size>;
    struct holes_t : holes_base_t {
        using holes_base_t::holes_base_t;
    };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        low_level_t,
        collector_t,
        horner_t,
        factor_t,
        power_t,
        splitter_t,
        like_terms_t>;
    struct local_rules_t : local_rules_base_t {
        using local_rules_base_t::local_rules_base_t;
    };
    using root_rules_base_t = proposer_union<
        holes_t>;
    struct root_rules_t : root_rules_base_t {
        using root_rules_base_t::root_rules_base_t;
    };
    struct subterms_t : subterm_proposer<local_rules_t, expr_pool> {
        using subterm_proposer<local_rules_t, expr_pool>::subterm_proposer;
    };
    struct rules_t : proposer_union<subterms_t, root_rules_t> {
        using proposer_union<subterms_t, root_rules_t>::proposer_union;
    };

    poly_rules_manifest(kernel_t& kernel);

    // rules
    low_level_t low_level_;
    collector_t collector_;
    horner_t horner_;
    factor_t factor_;
    power_t power_;
    splitter_t splitter_;
    like_terms_t like_terms_;
    holes_t holes_;
    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
private:
    static std::vector<uint32_t> intern_hole_symbols(kernel_t& kernel);
};

#endif
