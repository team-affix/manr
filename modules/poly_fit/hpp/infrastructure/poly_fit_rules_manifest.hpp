#ifndef POLY_FIT_RULES_MANIFEST_HPP
#define POLY_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/abs_evaluator.hpp"
#include "infrastructure/abs_indices.hpp"
#include "infrastructure/abs_reducer.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/poly_fit_hypotheses_manifest.hpp"
#include "infrastructure/poly_iteration_hypotheses.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_rules_manifest.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_assembler.hpp"
#include "infrastructure/pw_branch_absorber.hpp"
#include "infrastructure/pw_branch_filter.hpp"
#include "infrastructure/pw_builder.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_extension_binder.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting polynomial data, over a kernel: the samples become a piecewise with an open catch-all; the
// piecewise rules bind it to what `fit_` proposes (constants, interpolated polynomials, and piecewise polynomials
// split over intervals of one variable, periodic over residues, and over one variable a polynomial iterated with a
// helper, `let(lam(g(#0)), app(#0, app(#0, x)))`) and drop the branches they reproduce; and the full polynomial rule
// set compresses whatever polynomial results, all at every position. Expressions are evaluated through the
// abstraction reducer, so helpers reduce to polynomials before the polynomial evaluator sees them.
// `rules_` is what a search injects and `initial_` is where it starts.
struct poly_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    using fit_t = poly_fit_hypotheses_manifest;
    struct indices_t : abs_indices<symbol_table, expr_pool> {
        using abs_indices<symbol_table, expr_pool>::abs_indices;
    };
    struct reducer_t : abs_reducer<indices_t, indices_t, expr_pool> {
        using abs_reducer<indices_t, indices_t, expr_pool>::abs_reducer;
    };
    struct evaluator_t : abs_evaluator<reducer_t, fit_t::evaluator_t> {
        using abs_evaluator<reducer_t, fit_t::evaluator_t>::abs_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<evaluator_t, fit_t::conditions_t> {
        using pw_evaluator<evaluator_t, fit_t::conditions_t>::pw_evaluator;
    };
    struct iterations_t : poly_iteration_hypotheses<fit_t::converter_t, poly_interpolator, poly_interpolator,
                                                    kernel_t::renderer_t, indices_t, expr_pool> {
        using poly_iteration_hypotheses<fit_t::converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                        indices_t, expr_pool>::poly_iteration_hypotheses;
    };
    struct hypotheses_t : pw_hypotheses_union<fit_t::hypotheses_t, iterations_t> {
        using pw_hypotheses_union<fit_t::hypotheses_t, iterations_t>::pw_hypotheses_union;
    };
    struct assembler_t : pw_assembler<expr_pool> {
        using pw_assembler<expr_pool>::pw_assembler;
    };
    struct builder_t : pw_builder<expr_pool, expr_pool, assembler_t> {
        using pw_builder<expr_pool, expr_pool, assembler_t>::pw_builder;
    };
    struct filter_t : pw_branch_filter<pw_evaluator_t> {
        using pw_branch_filter<pw_evaluator_t>::pw_branch_filter;
    };
    struct binder_t : pw_extension_binder<hypotheses_t, filter_t, assembler_t> {
        using pw_extension_binder<hypotheses_t, filter_t, assembler_t>::pw_extension_binder;
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
    abs_symbols abs_symbols_;
    indices_t indices_;
    reducer_t reducer_;
    evaluator_t evaluator_;
    pw_evaluator_t pw_evaluator_;
    iterations_t iterations_;
    hypotheses_t hypotheses_;
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
