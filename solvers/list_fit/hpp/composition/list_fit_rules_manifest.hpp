#ifndef LIST_FIT_RULES_MANIFEST_HPP
#define LIST_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <vector>
#include "composition/logic_fit_hypotheses_manifest.hpp"
#include "composition/poly_fit_hypotheses_manifest.hpp"
#include "composition/poly_kernel_manifest.hpp"
#include "composition/poly_rules_manifest.hpp"
#include "composition/pw_fit_rules_manifest.hpp"
#include "hypotheses/list_affine_fold_hypotheses.hpp"
#include "hypotheses/list_count_fold_hypotheses.hpp"
#include "hypotheses/list_extremum_fold_hypotheses.hpp"
#include "hypotheses/list_factor_fold_hypotheses.hpp"
#include "hypotheses/list_filter_hypotheses.hpp"
#include "hypotheses/list_fold_hypotheses.hpp"
#include "hypotheses/list_integer_fold_hypotheses.hpp"
#include "hypotheses/list_integer_map_hypotheses.hpp"
#include "hypotheses/list_listed_fold_hypotheses.hpp"
#include "hypotheses/list_map_filter_hypotheses.hpp"
#include "hypotheses/list_map_hypotheses.hpp"
#include "hypotheses/list_product_fold_hypotheses.hpp"
#include "hypotheses/list_reverse_hypotheses.hpp"
#include "hypotheses/list_selection_fold_hypotheses.hpp"
#include "hypotheses/list_sum_fold_hypotheses.hpp"
#include "hypotheses/logic_condition_hypotheses.hpp"
#include "hypotheses/num_prime_hypotheses.hpp"
#include "hypotheses/pw_constant_hypotheses.hpp"
#include "hypotheses/pw_guarded_hypotheses.hpp"
#include "infrastructure/abs_applying_evaluator.hpp"
#include "infrastructure/abs_binders.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/list_affine_fold_solver.hpp"
#include "infrastructure/list_evaluator.hpp"
#include "infrastructure/list_factor_fold_solver.hpp"
#include "infrastructure/list_sample_converter.hpp"
#include "infrastructure/list_sum_fold_solver.hpp"
#include "infrastructure/list_values.hpp"
#include "infrastructure/logic_condition_evaluator.hpp"
#include "infrastructure/num_condition_atoms.hpp"
#include "infrastructure/num_evaluator.hpp"
#include "infrastructure/num_prime_table.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/list_sample.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/num_symbols.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting list programs over integer elements, on a polynomial kernel: the samples become a piecewise
// with an open catch-all; the piecewise rules bind it to constants and to the programs the list detectors read off the
// data (`map`, `fold`, `filter`), and drop the branches they reproduce. The generic map and fold detectors read
// elements and results of any domain; beside them, the integer cases (`list_integer_map_hypotheses`,
// `list_integer_fold_hypotheses`, the threshold filter) cover common integer data directly, and the affine, extremum
// (largest or smallest element), product and count fold detectors read folds from whole lists when the data holds no
// tails. The map-filter detector reads a filter composed under a map. The selection and factor fold detectors are the
// generic outer-inner readings (`docs/decomposing-detection.md`): the largest or smallest value, or the product, of an
// unknown inner function of the elements, handed to the element domain beside the extremum and product detectors, and
// the sum fold detector the same for the sum (a count on any condition when the inner function is 0 or 1). The listed
// fold detector reads folds that build lists by enumerating their small steps, and the reverse detector proposes the
// primitive `reverse(#0)`. The list detectors hand the element-level part of the problem to polynomial fitting:
// `element_fit_` over `#0` for map bodies, `step_fit_` over `#0` (the head) and `#1` (the accumulator) for fold steps.
// The full polynomial rule set then compresses the bodies, all at every position. `rules_` is what a search injects and
// `initial_` is where it starts.
// The most atoms a filter condition combines (the placeholder inputs of its truth table).
constexpr size_t k_list_condition_atoms = 6;

struct list_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    // ---- support: what the hypotheses and rules use ----
    using fit_t = poly_fit_hypotheses_manifest;
    struct values_t : list_values<expr_pool> {
        using list_values<expr_pool>::list_values;
    };
    struct binders_t : abs_binders<kernel_t::literals_t> {
        using abs_binders<kernel_t::literals_t>::abs_binders;
    };
    struct function_maker_t : abs_function_maker<kernel_t::literals_t, expr_pool> {
        using abs_function_maker<kernel_t::literals_t, expr_pool>::abs_function_maker;
    };
    struct numbers_t : num_evaluator<fit_t::pw_evaluator_t, fit_t::conditions_t, kernel_t::literals_t, kernel_t::literals_t, num_prime_table,
                                     num_prime_table> {
        using num_evaluator<fit_t::pw_evaluator_t, fit_t::conditions_t, kernel_t::literals_t, kernel_t::literals_t, num_prime_table,
                            num_prime_table>::num_evaluator;
    };
    struct conditions_t : logic_condition_evaluator<numbers_t> {
        using logic_condition_evaluator<numbers_t>::logic_condition_evaluator;
    };
    struct element_values_t : pw_evaluator<numbers_t, conditions_t> {
        using pw_evaluator<numbers_t, conditions_t>::pw_evaluator;
    };
    struct step_numbers_t : num_evaluator<fit_t::pw_evaluator_t, fit_t::conditions_t, kernel_t::literals_t, kernel_t::literals_t, num_prime_table,
                                          num_prime_table> {
        using num_evaluator<fit_t::pw_evaluator_t, fit_t::conditions_t, kernel_t::literals_t, kernel_t::literals_t, num_prime_table,
                            num_prime_table>::num_evaluator;
    };
    struct step_conditions_t : logic_condition_evaluator<step_numbers_t> {
        using logic_condition_evaluator<step_numbers_t>::logic_condition_evaluator;
    };
    struct step_values_t : pw_evaluator<step_numbers_t, step_conditions_t> {
        using pw_evaluator<step_numbers_t, step_conditions_t>::pw_evaluator;
    };
    using evaluator_base_t = list_evaluator<values_t, values_t, expr_pool, kernel_t::literals_t, element_values_t,
                                            conditions_t, step_values_t, binders_t, binders_t>;
    struct evaluator_t : evaluator_base_t {
        using evaluator_base_t::evaluator_base_t;
    };
    struct applying_t : abs_applying_evaluator<binders_t, evaluator_t> {
        using abs_applying_evaluator<binders_t, evaluator_t>::abs_applying_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<applying_t, evaluator_t> {
        using pw_evaluator<applying_t, evaluator_t>::pw_evaluator;
    };
    struct converter_t : list_sample_converter<values_t, kernel_t::literals_t> {
        using list_sample_converter<values_t, kernel_t::literals_t>::list_sample_converter;
    };

    // ---- hypotheses: add one's type here, then to `hypotheses_t` ----
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    struct primes_t : num_prime_hypotheses<kernel_t::literals_t, num_prime_table, num_prime_table, expr_pool> {
        using num_prime_hypotheses<kernel_t::literals_t, num_prime_table, num_prime_table, expr_pool>::num_prime_hypotheses;
    };
    struct atoms_t : num_condition_atoms<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using num_condition_atoms<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::num_condition_atoms;
    };
    using formulas_t = logic_fit_hypotheses_manifest::hypotheses_t;
    using combinations_base_t = logic_condition_hypotheses<atoms_t, conditions_t, formulas_t, expr_size,
                                                           kernel_t::literals_t, kernel_t::literals_t, expr_pool>;
    struct combinations_t : combinations_base_t {
        using combinations_base_t::combinations_base_t;
    };
    struct conditions_union_t : pw_hypotheses_union<primes_t, combinations_t> {
        using pw_hypotheses_union<primes_t, combinations_t>::pw_hypotheses_union;
    };
    using guarded_base_t = pw_guarded_hypotheses<conditions_union_t, atoms_t, fit_t::hypotheses_t, fit_t::interpolations_t, element_values_t, expr_size,
                                                 kernel_t::literals_t, expr_pool>;
    struct guarded_t : guarded_base_t {
        using guarded_base_t::guarded_base_t;
    };
    using element_union_t = guarded_t;
    struct integer_maps_t : list_integer_map_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t,
                                                        element_union_t, expr_pool> {
        using list_integer_map_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t, element_union_t,
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
    struct filterings_t : list_filter_hypotheses<values_t, kernel_t::literals_t, kernel_t::literals_t, expr_pool,
                                                 conditions_union_t> {
        using list_filter_hypotheses<values_t, kernel_t::literals_t, kernel_t::literals_t, expr_pool,
                                     conditions_union_t>::list_filter_hypotheses;
    };
    using map_filters_base_t =
        list_map_filter_hypotheses<values_t, kernel_t::literals_t, fit_t::converter_t, fit_t::hypotheses_t,
                                   conditions_union_t, kernel_t::literals_t, expr_pool>;
    struct map_filters_t : map_filters_base_t {
        using map_filters_base_t::map_filters_base_t;
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
                                 fit_t::converter_t, element_union_t, kernel_t::literals_t, expr_pool>;
    struct sum_folds_t : sum_folds_base_t {
        using sum_folds_base_t::sum_folds_base_t;
    };
    struct listed_folds_t : list_listed_fold_hypotheses<values_t, expr_pool> {
        using list_listed_fold_hypotheses<values_t, expr_pool>::list_listed_fold_hypotheses;
    };
    struct reversals_t : list_reverse_hypotheses<values_t, expr_pool> {
        using list_reverse_hypotheses<values_t, expr_pool>::list_reverse_hypotheses;
    };
    using hypotheses_base_t = pw_hypotheses_union<
        constants_t,
        integer_maps_t,
        integer_folds_t,
        filterings_t,
        maps_t,
        folds_t,
        affine_folds_t,
        extremum_folds_t,
        product_folds_t,
        count_folds_t,
        map_filters_t,
        selection_folds_t,
        factor_folds_t,
        sum_folds_t,
        listed_folds_t,
        reversals_t>;
    struct hypotheses_t : hypotheses_base_t {
        using hypotheses_base_t::hypotheses_base_t;
    };

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct piecewise_t : pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t, kernel_t::literals_t> {
        using pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t,
                                    kernel_t::literals_t>::pw_fit_rules_manifest;
    };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        piecewise_t::binder_t,
        piecewise_t::absorber_t,
        poly_rules_manifest::local_rules_t>;
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

    list_fit_rules_manifest(kernel_t& kernel,
                            const std::vector<list_sample>& samples,
                            const poly_fit_limits& fit_limits);

    poly_rules_manifest poly_rules_;
    pw_symbols pw_symbols_;
    abs_symbols abs_symbols_;
    list_symbols list_symbols_;
    num_symbols num_symbols_;
    logic_symbols logic_symbols_;
    uint32_t point_symbol_;
    uint32_t less_symbol_;
    fit_t element_fit_;
    fit_t step_fit_;
    values_t values_;
    binders_t binders_;
    function_maker_t function_maker_;
    num_prime_table prime_table_;
    numbers_t numbers_;
    conditions_t conditions_;
    element_values_t element_values_;
    step_numbers_t step_numbers_;
    step_conditions_t step_conditions_;
    step_values_t step_values_;
    evaluator_t evaluator_;
    applying_t applying_;
    pw_evaluator_t pw_evaluator_;
    converter_t converter_;
    constants_t constants_;
    primes_t primes_;
    atoms_t atoms_;
    logic_fit_hypotheses_manifest condition_logic_;
    combinations_t combinations_;
    conditions_union_t conditions_union_;
    guarded_t guarded_;
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
    listed_folds_t listed_folds_;
    reversals_t reversals_;
    hypotheses_t hypotheses_;
    // rules
    piecewise_t piecewise_;

    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
    const expr* initial_;
private:
    static std::vector<uint32_t> intern_placeholders(kernel_t& kernel);
};

#endif
