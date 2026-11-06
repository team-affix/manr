#ifndef POLY_RULES_MANIFEST_HPP
#define POLY_RULES_MANIFEST_HPP

#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/poly_collector.hpp"
#include "infrastructure/poly_component_splitter.hpp"
#include "infrastructure/poly_factor_detector.hpp"
#include "infrastructure/poly_horner_detector.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_like_terms.hpp"
#include "infrastructure/poly_local_rules.hpp"
#include "infrastructure/poly_power_detector.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/subterm_proposer.hpp"

// The full polynomial rule set over a kernel: the low-level local rules plus every detector, applied at every
// position. `rules_` is what a search injects.
struct poly_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    struct local_t : poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::poly_local_rules;
    };
    struct collector_t : poly_collector<kernel_t::expander_t, kernel_t::renderer_t, expr_size> {
        using poly_collector<kernel_t::expander_t, kernel_t::renderer_t, expr_size>::poly_collector;
    };
    struct horner_t : poly_horner_detector<kernel_t::expander_t, kernel_t::horner_t, expr_size> {
        using poly_horner_detector<kernel_t::expander_t, kernel_t::horner_t, expr_size>::poly_horner_detector;
    };
    struct factor_t : poly_factor_detector<kernel_t::expander_t, poly_arithmetic, poly_linear_factor_finder,
                                           kernel_t::renderer_t, expr_pool, expr_size> {
        using poly_factor_detector<kernel_t::expander_t, poly_arithmetic, poly_linear_factor_finder,
                                   kernel_t::renderer_t, expr_pool, expr_size>::poly_factor_detector;
    };
    struct power_t : poly_power_detector<kernel_t::expander_t, kernel_t::roots_t, kernel_t::renderer_t, expr_pool,
                                         kernel_t::literals_t, expr_size> {
        using poly_power_detector<kernel_t::expander_t, kernel_t::roots_t, kernel_t::renderer_t, expr_pool,
                                  kernel_t::literals_t, expr_size>::poly_power_detector;
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
    struct union_t
        : proposer_union<local_t, collector_t, horner_t, factor_t, power_t, splitter_t, like_terms_t> {
        using proposer_union<local_t, collector_t, horner_t, factor_t, power_t, splitter_t, like_terms_t>::proposer_union;
    };
    struct rules_t : subterm_proposer<union_t, expr_pool> {
        using subterm_proposer<union_t, expr_pool>::subterm_proposer;
    };

    poly_rules_manifest(kernel_t& kernel);

    local_t local_;
    collector_t collector_;
    horner_t horner_;
    factor_t factor_;
    power_t power_;
    splitter_t splitter_;
    like_terms_t like_terms_;
    union_t union_;
    rules_t rules_;
};

#endif
