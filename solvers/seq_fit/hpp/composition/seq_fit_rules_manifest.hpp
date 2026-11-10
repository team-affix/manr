#ifndef SEQ_FIT_RULES_MANIFEST_HPP
#define SEQ_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "composition/poly_fit_hypotheses_manifest.hpp"
#include "composition/poly_kernel_manifest.hpp"
#include "composition/poly_rules_manifest.hpp"
#include "composition/pw_fit_rules_manifest.hpp"
#include "hypotheses/num_prime_hypotheses.hpp"
#include "hypotheses/seq_recurrence_hypotheses.hpp"
#include "infrastructure/abs_applying_evaluator.hpp"
#include "infrastructure/abs_binders.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/num_evaluator.hpp"
#include "infrastructure/num_prime_table.hpp"
#include "infrastructure/poly_condition_evaluator.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/seq_evaluator.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "rules/abs_binder_merger.hpp"
#include "rules/seq_open_list_reader.hpp"
#include "rules/seq_power_closer.hpp"
#include "rules/seq_unfolder.hpp"
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
    // ---- support: what the hypotheses and rules use ----
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

    // ---- hypotheses: add one's type here, then to `hypotheses_t` ----
    using recurrences_base_t = seq_recurrence_hypotheses<kernel_t::literals_t, fit_t::converter_t,
                                                         fit_t::interpolations_t, kernel_t::literals_t, expr_pool,
                                                         fit_t::evaluator_t>;
    struct recurrences_t : recurrences_base_t {
        using recurrences_base_t::recurrences_base_t;
    };
    struct primes_t : num_prime_hypotheses<kernel_t::literals_t, num_prime_table, num_prime_table, expr_pool> {
        using num_prime_hypotheses<kernel_t::literals_t, num_prime_table, num_prime_table, expr_pool>::num_prime_hypotheses;
    };
    using hypotheses_base_t = pw_hypotheses_union<
        fit_t::hypotheses_t,
        recurrences_t,
        recurrences_t,
        recurrences_t,
        primes_t>;
    struct hypotheses_t : hypotheses_base_t {
        using hypotheses_base_t::hypotheses_base_t;
    };

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct piecewise_t : pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t, kernel_t::literals_t> {
        using pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t,
                                    kernel_t::literals_t>::pw_fit_rules_manifest;
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
    struct open_lists_t : seq_open_list_reader<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using seq_open_list_reader<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::seq_open_list_reader;
    };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        piecewise_t::binder_t,
        piecewise_t::absorber_t,
        closer_t,
        unfolder_t,
        merger_t,
        poly_rules_manifest::local_rules_t>;
    struct local_rules_t : local_rules_base_t {
        using local_rules_base_t::local_rules_base_t;
    };
    using root_rules_base_t = proposer_union<
        open_lists_t>;
    struct root_rules_t : root_rules_base_t {
        using root_rules_base_t::root_rules_base_t;
    };
    struct subterms_t : subterm_proposer<local_rules_t, expr_pool> {
        using subterm_proposer<local_rules_t, expr_pool>::subterm_proposer;
    };
    struct rules_t : proposer_union<subterms_t, root_rules_t> {
        using proposer_union<subterms_t, root_rules_t>::proposer_union;
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
    // rules
    piecewise_t piecewise_;
    closer_t closer_;
    unfolder_t unfolder_;
    merger_t merger_;
    open_lists_t open_lists_;
    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
    const expr* initial_;
};

#endif
