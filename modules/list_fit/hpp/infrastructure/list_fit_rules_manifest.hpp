#ifndef LIST_FIT_RULES_MANIFEST_HPP
#define LIST_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <vector>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/list_affine_fold_hypotheses.hpp"
#include "infrastructure/list_affine_fold_solver.hpp"
#include "infrastructure/list_count_fold_hypotheses.hpp"
#include "infrastructure/list_evaluator.hpp"
#include "infrastructure/list_extremum_fold_hypotheses.hpp"
#include "infrastructure/list_factor_fold_hypotheses.hpp"
#include "infrastructure/list_factor_fold_solver.hpp"
#include "infrastructure/list_filter_hypotheses.hpp"
#include "infrastructure/list_fold_hypotheses.hpp"
#include "infrastructure/list_integer_fold_hypotheses.hpp"
#include "infrastructure/list_integer_map_hypotheses.hpp"
#include "infrastructure/list_map_filter_hypotheses.hpp"
#include "infrastructure/list_map_hypotheses.hpp"
#include "infrastructure/list_product_fold_hypotheses.hpp"
#include "infrastructure/list_sample_converter.hpp"
#include "infrastructure/list_selection_fold_hypotheses.hpp"
#include "infrastructure/list_sum_fold_hypotheses.hpp"
#include "infrastructure/list_sum_fold_solver.hpp"
#include "infrastructure/list_values.hpp"
#include "infrastructure/poly_fit_hypotheses_manifest.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_rules_manifest.hpp"
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
#include "value_objects/list_sample.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting list programs over integer elements, on a polynomial kernel: the samples become a piecewise
// with an open catch-all; the piecewise rules bind it to constants and to the programs the list detectors read off
// the data (`map`, `fold`, `filter`), and drop the branches they reproduce. The generic map and fold detectors read
// elements and results of any domain; beside them, the integer cases (`list_integer_map_hypotheses`,
// `list_integer_fold_hypotheses`, the threshold filter) cover common integer data directly, and the affine, extremum
// (largest or smallest element), product and count fold detectors read folds from whole lists when the data holds no
// tails. The map-filter detector reads a filter composed under a map. The selection and factor fold detectors are the
// generic outer-inner readings (`docs/decomposing-detection.md`): the largest or smallest value, or the product, of an
// unknown inner function of the elements, handed to the element domain beside the extremum and product detectors, and
// the sum fold detector the same for the sum (a count on any condition when the inner function is 0 or 1).
// The list detectors hand the element-level part of the problem to polynomial fitting: `element_fit_` over `#0` for
// map bodies, `step_fit_` over `#0` (the head) and `#1` (the accumulator) for fold steps. The full polynomial rule set then compresses the bodies,
// all at every position. `rules_` is what a search injects and `initial_` is where it starts.
struct list_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    using fit_t = poly_fit_hypotheses_manifest;
    struct values_t : list_values<expr_pool> {
        using list_values<expr_pool>::list_values;
    };
    using evaluator_base_t = list_evaluator<values_t, values_t, expr_pool, kernel_t::literals_t, fit_t::pw_evaluator_t,
                                            fit_t::conditions_t, fit_t::pw_evaluator_t>;
    struct evaluator_t : evaluator_base_t {
        using evaluator_base_t::evaluator_base_t;
    };
    struct pw_evaluator_t : pw_evaluator<evaluator_t, evaluator_t> {
        using pw_evaluator<evaluator_t, evaluator_t>::pw_evaluator;
    };
    struct converter_t : list_sample_converter<values_t, kernel_t::literals_t> {
        using list_sample_converter<values_t, kernel_t::literals_t>::list_sample_converter;
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
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    struct integer_maps_t : list_integer_map_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t,
                                                        fit_t::hypotheses_t, expr_pool> {
        using list_integer_map_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t, fit_t::hypotheses_t,
                                          expr_pool>::list_integer_map_hypotheses;
    };
    struct integer_folds_t
        : list_integer_fold_hypotheses<kernel_t::literals_t, fit_t::converter_t, fit_t::hypotheses_t, expr_pool> {
        using list_integer_fold_hypotheses<kernel_t::literals_t, fit_t::converter_t, fit_t::hypotheses_t,
                                           expr_pool>::list_integer_fold_hypotheses;
    };
    struct maps_t : list_map_hypotheses<values_t, fit_t::hypotheses_t, expr_pool> {
        using list_map_hypotheses<values_t, fit_t::hypotheses_t, expr_pool>::list_map_hypotheses;
    };
    struct folds_t : list_fold_hypotheses<fit_t::hypotheses_t, expr_pool> {
        using list_fold_hypotheses<fit_t::hypotheses_t, expr_pool>::list_fold_hypotheses;
    };
    struct affine_folds_t : list_affine_fold_hypotheses<values_t, kernel_t::literals_t, list_affine_fold_solver,
                                                        list_affine_fold_solver, kernel_t::renderer_t,
                                                        kernel_t::literals_t, expr_pool> {
        using list_affine_fold_hypotheses<values_t, kernel_t::literals_t, list_affine_fold_solver,
                                          list_affine_fold_solver, kernel_t::renderer_t, kernel_t::literals_t,
                                          expr_pool>::list_affine_fold_hypotheses;
    };
    struct extremum_folds_t
        : list_extremum_fold_hypotheses<values_t, kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using list_extremum_fold_hypotheses<values_t, kernel_t::literals_t, kernel_t::literals_t,
                                            expr_pool>::list_extremum_fold_hypotheses;
    };
    struct product_folds_t : list_product_fold_hypotheses<values_t, kernel_t::literals_t, kernel_t::renderer_t,
                                                          kernel_t::literals_t, expr_pool> {
        using list_product_fold_hypotheses<values_t, kernel_t::literals_t, kernel_t::renderer_t, kernel_t::literals_t,
                                           expr_pool>::list_product_fold_hypotheses;
    };
    struct count_folds_t : list_count_fold_hypotheses<values_t, kernel_t::literals_t, kernel_t::renderer_t,
                                                      kernel_t::literals_t, expr_pool> {
        using list_count_fold_hypotheses<values_t, kernel_t::literals_t, kernel_t::renderer_t, kernel_t::literals_t,
                                         expr_pool>::list_count_fold_hypotheses;
    };
    struct filterings_t
        : list_filter_hypotheses<values_t, kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using list_filter_hypotheses<values_t, kernel_t::literals_t, kernel_t::literals_t,
                                     expr_pool>::list_filter_hypotheses;
    };
    struct map_filters_t : list_map_filter_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t,
                                                      fit_t::hypotheses_t, kernel_t::literals_t, expr_pool> {
        using list_map_filter_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t, fit_t::hypotheses_t,
                                         kernel_t::literals_t, expr_pool>::list_map_filter_hypotheses;
    };
    using selection_folds_base_t =
        list_selection_fold_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t, fit_t::hypotheses_t,
                                       kernel_t::literals_t, expr_pool>;
    struct selection_folds_t : selection_folds_base_t {
        using selection_folds_base_t::selection_folds_base_t;
    };
    using factor_folds_base_t =
        list_factor_fold_hypotheses<values_t, kernel_t::literals_t, list_factor_fold_solver, list_factor_fold_solver,
                                    fit_t::converter_t, fit_t::hypotheses_t, kernel_t::literals_t, expr_pool>;
    struct factor_folds_t : factor_folds_base_t {
        using factor_folds_base_t::factor_folds_base_t;
    };
    using sum_folds_base_t =
        list_sum_fold_hypotheses<values_t, kernel_t::literals_t, list_sum_fold_solver, list_sum_fold_solver,
                                 fit_t::converter_t, fit_t::hypotheses_t, kernel_t::literals_t, expr_pool>;
    struct sum_folds_t : sum_folds_base_t {
        using sum_folds_base_t::sum_folds_base_t;
    };
    struct hypotheses_t : pw_hypotheses_union<constants_t, integer_maps_t, integer_folds_t, filterings_t, maps_t,
                                             folds_t, affine_folds_t, extremum_folds_t, product_folds_t,
                                             count_folds_t, map_filters_t, selection_folds_t, factor_folds_t,
                                             sum_folds_t> {
        using pw_hypotheses_union<constants_t, integer_maps_t, integer_folds_t, filterings_t, maps_t, folds_t,
                                  affine_folds_t, extremum_folds_t, product_folds_t, count_folds_t, map_filters_t,
                                  selection_folds_t, factor_folds_t, sum_folds_t>::pw_hypotheses_union;
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

    list_fit_rules_manifest(kernel_t& kernel, const std::vector<list_sample>& samples, const poly_fit_limits& fit_limits);

    poly_rules_manifest poly_rules_;
    pw_symbols pw_symbols_;
    list_symbols list_symbols_;
    uint32_t point_symbol_;
    uint32_t less_symbol_;
    fit_t element_fit_;
    fit_t step_fit_;
    values_t values_;
    evaluator_t evaluator_;
    pw_evaluator_t pw_evaluator_;
    converter_t converter_;
    assembler_t assembler_;
    builder_t builder_;
    filter_t filter_;
    constants_t constants_;
    integer_maps_t integer_maps_;
    integer_folds_t integer_folds_;
    filterings_t filterings_;
    maps_t maps_;
    folds_t folds_;
    list_affine_fold_solver affine_solver_;
    affine_folds_t affine_folds_;
    extremum_folds_t extremum_folds_;
    product_folds_t product_folds_;
    count_folds_t count_folds_;
    map_filters_t map_filters_;
    selection_folds_t selection_folds_;
    list_factor_fold_solver factor_solver_;
    factor_folds_t factor_folds_;
    list_sum_fold_solver sum_solver_;
    sum_folds_t sum_folds_;
    hypotheses_t hypotheses_;
    binder_t binder_;
    absorber_t absorber_;
    union_t union_;
    rules_t rules_;
    const expr* initial_;
};

#endif
