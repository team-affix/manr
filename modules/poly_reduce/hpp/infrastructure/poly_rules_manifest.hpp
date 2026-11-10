#ifndef POLY_RULES_MANIFEST_HPP
#define POLY_RULES_MANIFEST_HPP

#include <cstdint>
#include <vector>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/poly_collector.hpp"
#include "infrastructure/poly_component_splitter.hpp"
#include "infrastructure/poly_factor_detector.hpp"
#include "infrastructure/poly_hole_binder.hpp"
#include "infrastructure/poly_horner_detector.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_like_terms.hpp"
#include "infrastructure/poly_local_rules.hpp"
#include "infrastructure/poly_power_detector.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/subterm_proposer.hpp"

// The full polynomial rule set over a kernel: the low-level local rules plus every detector, applied at every
// position, and the hole binder, applied at the root (a binding substitutes everywhere). `rules_` is what a search
// injects.
struct poly_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    struct local_t : poly_local_rules<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
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
    struct union_t
        : proposer_union<local_t, collector_t, horner_t, factor_t, power_t, splitter_t, like_terms_t> {
        using proposer_union<local_t, collector_t, horner_t, factor_t, power_t, splitter_t, like_terms_t>::proposer_union;
    };
    struct subterms_t : subterm_proposer<union_t, expr_pool> {
        using subterm_proposer<union_t, expr_pool>::subterm_proposer;
    };
    using holes_base_t = poly_hole_binder<kernel_t::expander_t, kernel_t::renderer_t, kernel_t::renderer_t,
                                          poly_arithmetic, poly_arithmetic, poly_arithmetic, poly_arithmetic,
                                          kernel_t::roots_t, expr_pool, expr_pool, expr_size>;
    struct holes_t : holes_base_t {
        using holes_base_t::holes_base_t;
    };
    struct rules_t : proposer_union<subterms_t, holes_t> {
        using proposer_union<subterms_t, holes_t>::proposer_union;
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
    subterms_t subterms_;
    holes_t holes_;
    rules_t rules_;
private:
    static std::vector<uint32_t> intern_hole_symbols(kernel_t& kernel);
};

#endif
