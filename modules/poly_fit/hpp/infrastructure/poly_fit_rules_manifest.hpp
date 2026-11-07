#ifndef POLY_FIT_RULES_MANIFEST_HPP
#define POLY_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/poly_condition_evaluator.hpp"
#include "infrastructure/poly_interpolation_hypotheses.hpp"
#include "infrastructure/poly_interpolator.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_point_evaluator.hpp"
#include "infrastructure/poly_rules_manifest.hpp"
#include "infrastructure/poly_sample_converter.hpp"
#include "infrastructure/poly_split_hypotheses.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_assembler.hpp"
#include "infrastructure/pw_branch_absorber.hpp"
#include "infrastructure/pw_branch_filter.hpp"
#include "infrastructure/pw_builder.hpp"
#include "infrastructure/pw_constant_hypotheses.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_extension_binder.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting polynomial data, over a kernel: the samples become a piecewise with an open catch-all; the
// piecewise rules bind it to constants, to polynomials interpolated from the data, and to piecewise polynomials
// split over intervals of one variable (`when(lt(x, k), ...)` branches), and drop the branches they reproduce; and the full polynomial rule set compresses whatever polynomial results, all at every position.
// `rules_` is what a search injects and `initial_` is where it starts.
struct poly_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    struct converter_t : poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t> {
        using poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t>::poly_sample_converter;
    };
    struct evaluator_t : poly_point_evaluator<kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t> {
        using poly_point_evaluator<kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t>::poly_point_evaluator;
    };
    struct conditions_t : poly_condition_evaluator<evaluator_t, kernel_t::literals_t> {
        using poly_condition_evaluator<evaluator_t, kernel_t::literals_t>::poly_condition_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<evaluator_t, conditions_t> {
        using pw_evaluator<evaluator_t, conditions_t>::pw_evaluator;
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
    struct interpolations_t
        : poly_interpolation_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t> {
        using poly_interpolation_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t>::
            poly_interpolation_hypotheses;
    };
    struct splits_t : poly_split_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                            expr_size, kernel_t::literals_t, expr_pool> {
        using poly_split_hypotheses<converter_t, poly_interpolator, poly_interpolator, kernel_t::renderer_t,
                                    expr_size, kernel_t::literals_t, expr_pool>::poly_split_hypotheses;
    };
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    struct hypotheses_t : pw_hypotheses_union<constants_t, interpolations_t, splits_t> {
        using pw_hypotheses_union<constants_t, interpolations_t, splits_t>::pw_hypotheses_union;
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
    std::vector<uint32_t> variables_;
    converter_t converter_;
    poly_interpolator interpolator_;
    evaluator_t evaluator_;
    conditions_t conditions_;
    pw_evaluator_t pw_evaluator_;
    assembler_t assembler_;
    builder_t builder_;
    filter_t filter_;
    constants_t constants_;
    interpolations_t interpolations_;
    splits_t splits_;
    hypotheses_t hypotheses_;
    binder_t binder_;
    absorber_t absorber_;
    union_t union_;
    rules_t rules_;
    const expr* initial_;
private:
    static std::vector<uint32_t> intern_variables(kernel_t& kernel, const std::vector<std::string>& variables);
};

#endif
