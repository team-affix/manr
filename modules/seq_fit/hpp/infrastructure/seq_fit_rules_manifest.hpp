#ifndef SEQ_FIT_RULES_MANIFEST_HPP
#define SEQ_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/abs_applying_evaluator.hpp"
#include "infrastructure/abs_binder_merger.hpp"
#include "infrastructure/abs_binders.hpp"
#include "infrastructure/abs_closing_hypotheses.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/arith_hole_solver.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/num_evaluator.hpp"
#include "infrastructure/num_prime_hypotheses.hpp"
#include "infrastructure/num_prime_table.hpp"
#include "infrastructure/poly_condition_evaluator.hpp"
#include "infrastructure/poly_fit_hypotheses_manifest.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_rules_manifest.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_assembler.hpp"
#include "infrastructure/pw_branch_absorber.hpp"
#include "infrastructure/pw_branch_filter.hpp"
#include "infrastructure/pw_builder.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_extension_binder.hpp"
#include "infrastructure/pw_hole_matcher.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/seq_evaluator.hpp"
#include "infrastructure/seq_open_list_reader.hpp"
#include "infrastructure/seq_power_closer.hpp"
#include "infrastructure/seq_recurrence_hypotheses.hpp"
#include "infrastructure/seq_unfolder.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/num_symbols.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"
#include "value_objects/seq_symbols.hpp"

// The rules for fitting integer sequences (data over one index variable), on a polynomial kernel: everything
// polynomial fitting does (`fit_`), plus the recurrences of order 1 to 3 read off consecutive terms, whose steps go to
// polynomial fitting over the index `#0` and the previous terms `#1` to `#3` (`first_steps_` to `third_steps_`), and
// the closing rule that turns a geometric recurrence into a power. The evaluator runs recurrences and powers and
// leaves the rest to the polynomial evaluator, and chains of binders merge into one `lams`. A piecewise is a
// function of the index: what replaces its catch-all is closed over it, `lam(b)` with the index `#0` in b, and a
// function is applied to the input when evaluated. An open-ended list (`cons(0, cons(1, ..., ?0))`) is read at the
// root as the infinite list of a sequence, `tabulate` of its data, and the infinite list of a recurrence closes into
// `unfold`. `rules_` is what a search injects and `initial_` is where it starts.
struct seq_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    using fit_t = poly_fit_hypotheses_manifest;
    struct binders_t : abs_binders<kernel_t::literals_t> {
        using abs_binders<kernel_t::literals_t>::abs_binders;
    };
    struct function_maker_t : abs_function_maker<kernel_t::literals_t, expr_pool> {
        using abs_function_maker<kernel_t::literals_t, expr_pool>::abs_function_maker;
    };
    using evaluator_base_t = seq_evaluator<fit_t::evaluator_t, fit_t::evaluator_t, binders_t, kernel_t::literals_t,
                                           kernel_t::literals_t, expr_pool>;
    struct evaluator_t : evaluator_base_t {
        using evaluator_base_t::evaluator_base_t;
    };
    struct conditions_t : poly_condition_evaluator<evaluator_t, kernel_t::literals_t> {
        using poly_condition_evaluator<evaluator_t, kernel_t::literals_t>::poly_condition_evaluator;
    };
    struct body_evaluator_t : pw_evaluator<evaluator_t, conditions_t> {
        using pw_evaluator<evaluator_t, conditions_t>::pw_evaluator;
    };
    struct numbers_t : num_evaluator<body_evaluator_t, conditions_t, kernel_t::literals_t, kernel_t::literals_t, num_prime_table,
                                     num_prime_table> {
        using num_evaluator<body_evaluator_t, conditions_t, kernel_t::literals_t, kernel_t::literals_t, num_prime_table,
                            num_prime_table>::num_evaluator;
    };
    struct applying_t : abs_applying_evaluator<binders_t, numbers_t> {
        using abs_applying_evaluator<binders_t, numbers_t>::abs_applying_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<applying_t, conditions_t> {
        using pw_evaluator<applying_t, conditions_t>::pw_evaluator;
    };
    using recurrences_base_t = seq_recurrence_hypotheses<kernel_t::literals_t, fit_t::converter_t,
                                                         fit_t::interpolations_t, kernel_t::literals_t, expr_pool,
                                                         fit_t::evaluator_t>;
    struct recurrences_t : recurrences_base_t {
        using recurrences_base_t::recurrences_base_t;
    };
    struct primes_t : num_prime_hypotheses<kernel_t::literals_t, num_prime_table, num_prime_table, expr_pool> {
        using num_prime_hypotheses<kernel_t::literals_t, num_prime_table, num_prime_table, expr_pool>::num_prime_hypotheses;
    };
    struct hypotheses_t
        : pw_hypotheses_union<fit_t::hypotheses_t, recurrences_t, recurrences_t, recurrences_t, primes_t> {
        using pw_hypotheses_union<fit_t::hypotheses_t, recurrences_t, recurrences_t, recurrences_t,
                                  primes_t>::pw_hypotheses_union;
    };
    struct closing_t : abs_closing_hypotheses<hypotheses_t, function_maker_t> {
        using abs_closing_hypotheses<hypotheses_t, function_maker_t>::abs_closing_hypotheses;
    };
    struct assembler_t : pw_assembler<expr_pool> {
        using pw_assembler<expr_pool>::pw_assembler;
    };
    struct builder_t : pw_builder<expr_pool, expr_pool, assembler_t> {
        using pw_builder<expr_pool, expr_pool, assembler_t>::pw_builder;
    };
    struct solver_t : arith_hole_solver<kernel_t::literals_t, kernel_t::literals_t> {
        using arith_hole_solver<kernel_t::literals_t, kernel_t::literals_t>::arith_hole_solver;
    };
    struct matcher_t : pw_hole_matcher<solver_t> {
        using pw_hole_matcher<solver_t>::pw_hole_matcher;
    };
    struct filter_t : pw_branch_filter<pw_evaluator_t, matcher_t> {
        using pw_branch_filter<pw_evaluator_t, matcher_t>::pw_branch_filter;
    };
    struct binder_t : pw_extension_binder<closing_t, filter_t, assembler_t> {
        using pw_extension_binder<closing_t, filter_t, assembler_t>::pw_extension_binder;
    };
    struct absorber_t : pw_branch_absorber<filter_t, assembler_t> {
        using pw_branch_absorber<filter_t, assembler_t>::pw_branch_absorber;
    };
    struct closer_t : seq_power_closer<binders_t, kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t,
                                       expr_pool> {
        using seq_power_closer<binders_t, kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t,
                               expr_pool>::seq_power_closer;
    };
    struct merger_t : abs_binder_merger<binders_t, kernel_t::literals_t, expr_pool> {
        using abs_binder_merger<binders_t, kernel_t::literals_t, expr_pool>::abs_binder_merger;
    };
    struct unfolder_t : seq_unfolder<binders_t, expr_pool> {
        using seq_unfolder<binders_t, expr_pool>::seq_unfolder;
    };
    struct union_t
        : proposer_union<binder_t, absorber_t, closer_t, unfolder_t, merger_t, poly_rules_manifest::union_t> {
        using proposer_union<binder_t, absorber_t, closer_t, unfolder_t, merger_t,
                             poly_rules_manifest::union_t>::proposer_union;
    };
    struct subterms_t : subterm_proposer<union_t, expr_pool> {
        using subterm_proposer<union_t, expr_pool>::subterm_proposer;
    };
    struct open_lists_t : seq_open_list_reader<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using seq_open_list_reader<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::seq_open_list_reader;
    };
    struct rules_t : proposer_union<subterms_t, open_lists_t> {
        using proposer_union<subterms_t, open_lists_t>::proposer_union;
    };

    seq_fit_rules_manifest(kernel_t& kernel,
                           const std::vector<poly_sample>& samples,
                           const poly_fit_limits& fit_limits);

    poly_rules_manifest poly_rules_;
    pw_symbols pw_symbols_;
    abs_symbols abs_symbols_;
    seq_symbols seq_symbols_;
    num_symbols num_symbols_;
    uint32_t point_symbol_;
    uint32_t less_symbol_;
    fit_t fit_;
    fit_t first_steps_;
    fit_t second_steps_;
    fit_t third_steps_;
    num_prime_table prime_table_;
    binders_t binders_;
    function_maker_t function_maker_;
    evaluator_t evaluator_;
    conditions_t conditions_;
    body_evaluator_t body_evaluator_;
    numbers_t numbers_;
    applying_t applying_;
    pw_evaluator_t pw_evaluator_;
    recurrences_t first_order_;
    recurrences_t second_order_;
    recurrences_t third_order_;
    primes_t primes_;
    hypotheses_t hypotheses_;
    closing_t closing_;
    assembler_t assembler_;
    builder_t builder_;
    solver_t solver_;
    matcher_t matcher_;
    filter_t filter_;
    binder_t binder_;
    absorber_t absorber_;
    closer_t closer_;
    unfolder_t unfolder_;
    merger_t merger_;
    union_t union_;
    subterms_t subterms_;
    open_lists_t open_lists_;
    rules_t rules_;
    const expr* initial_;
};

#endif
