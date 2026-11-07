#ifndef POLY_FIT_RULES_MANIFEST_HPP
#define POLY_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/poly_fit_hypotheses_manifest.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_rules_manifest.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_assembler.hpp"
#include "infrastructure/pw_branch_absorber.hpp"
#include "infrastructure/pw_branch_filter.hpp"
#include "infrastructure/pw_builder.hpp"
#include "infrastructure/pw_extension_binder.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting polynomial data, over a kernel: the samples become a piecewise with an open catch-all; the
// piecewise rules bind it to what `fit_` proposes (constants, interpolated polynomials, and piecewise polynomials
// split over intervals of one variable) and drop the branches they reproduce; and the full polynomial rule set
// compresses whatever polynomial results, all at every position.
// `rules_` is what a search injects and `initial_` is where it starts.
struct poly_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    using fit_t = poly_fit_hypotheses_manifest;
    struct assembler_t : pw_assembler<expr_pool> {
        using pw_assembler<expr_pool>::pw_assembler;
    };
    struct builder_t : pw_builder<expr_pool, expr_pool, assembler_t> {
        using pw_builder<expr_pool, expr_pool, assembler_t>::pw_builder;
    };
    struct filter_t : pw_branch_filter<fit_t::pw_evaluator_t> {
        using pw_branch_filter<fit_t::pw_evaluator_t>::pw_branch_filter;
    };
    struct binder_t : pw_extension_binder<fit_t::hypotheses_t, filter_t, assembler_t> {
        using pw_extension_binder<fit_t::hypotheses_t, filter_t, assembler_t>::pw_extension_binder;
    };
    struct absorber_t : pw_branch_absorber<filter_t, assembler_t> {
        using pw_branch_absorber<filter_t, assembler_t>::pw_branch_absorber;
    };
    struct union_t : proposer_union<binder_t, absorber_t, poly_rules_manifest::union_t> {
        using proposer_union<binder_t, absorber_t, poly_rules_manifest::union_t>::proposer_union;
    };
    struct rules_t : subterm_proposer<union_t, expr_pool> {
        using subterm_proposer<union_t, expr_pool>::subterm_proposer;
    };

    poly_fit_rules_manifest(kernel_t& kernel,
                            const std::vector<std::string>& variables,
                            const std::vector<poly_sample>& samples,
                            const poly_fit_limits& fit_limits);

    poly_rules_manifest poly_rules_;
    pw_symbols pw_symbols_;
    uint32_t point_symbol_;
    uint32_t less_symbol_;
    fit_t fit_;
    assembler_t assembler_;
    builder_t builder_;
    filter_t filter_;
    binder_t binder_;
    absorber_t absorber_;
    union_t union_;
    rules_t rules_;
    const expr* initial_;
};

#endif
