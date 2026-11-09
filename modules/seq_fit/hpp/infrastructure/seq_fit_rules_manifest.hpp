#ifndef SEQ_FIT_RULES_MANIFEST_HPP
#define SEQ_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/expr_pool.hpp"
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
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/seq_evaluator.hpp"
#include "infrastructure/seq_power_closer.hpp"
#include "infrastructure/seq_recurrence_hypotheses.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/poly_fit_limits.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"
#include "value_objects/seq_symbols.hpp"

// The rules for fitting integer sequences (data over one index variable), on a polynomial kernel: everything
// polynomial fitting does (`fit_`), plus the recurrences of order 1 to 3 read off consecutive terms, whose steps go to
// polynomial fitting over the index `#0` and the previous terms `#1` to `#3` (`first_steps_` to `third_steps_`), and the closing rule that
// turns a geometric recurrence into a power. The evaluator runs recurrences and powers and leaves the rest to the
// polynomial evaluator. `rules_` is what a search injects and `initial_` is where it starts.
struct seq_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    using fit_t = poly_fit_hypotheses_manifest;
    using evaluator_base_t = seq_evaluator<fit_t::evaluator_t, fit_t::evaluator_t, kernel_t::literals_t,
                                           kernel_t::literals_t, expr_pool>;
    struct evaluator_t : evaluator_base_t {
        using evaluator_base_t::evaluator_base_t;
    };
    struct conditions_t : poly_condition_evaluator<evaluator_t, kernel_t::literals_t> {
        using poly_condition_evaluator<evaluator_t, kernel_t::literals_t>::poly_condition_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<evaluator_t, conditions_t> {
        using pw_evaluator<evaluator_t, conditions_t>::pw_evaluator;
    };
    using recurrences_base_t = seq_recurrence_hypotheses<kernel_t::literals_t, fit_t::converter_t,
                                                         fit_t::interpolations_t, kernel_t::literals_t, expr_pool>;
    struct recurrences_t : recurrences_base_t {
        using recurrences_base_t::recurrences_base_t;
    };
    struct hypotheses_t : pw_hypotheses_union<fit_t::hypotheses_t, recurrences_t, recurrences_t, recurrences_t> {
        using pw_hypotheses_union<fit_t::hypotheses_t, recurrences_t, recurrences_t,
                                  recurrences_t>::pw_hypotheses_union;
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
    struct closer_t
        : seq_power_closer<kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using seq_power_closer<kernel_t::expander_t, kernel_t::literals_t, kernel_t::literals_t,
                               expr_pool>::seq_power_closer;
    };
    struct union_t : proposer_union<binder_t, absorber_t, closer_t, poly_rules_manifest::union_t> {
        using proposer_union<binder_t, absorber_t, closer_t, poly_rules_manifest::union_t>::proposer_union;
    };
    struct rules_t : subterm_proposer<union_t, expr_pool> {
        using subterm_proposer<union_t, expr_pool>::subterm_proposer;
    };

    seq_fit_rules_manifest(kernel_t& kernel,
                           const std::string& variable,
                           const std::vector<poly_sample>& samples,
                           const poly_fit_limits& fit_limits);

    poly_rules_manifest poly_rules_;
    pw_symbols pw_symbols_;
    seq_symbols seq_symbols_;
    uint32_t point_symbol_;
    uint32_t less_symbol_;
    fit_t fit_;
    fit_t first_steps_;
    fit_t second_steps_;
    fit_t third_steps_;
    evaluator_t evaluator_;
    conditions_t conditions_;
    pw_evaluator_t pw_evaluator_;
    recurrences_t first_order_;
    recurrences_t second_order_;
    recurrences_t third_order_;
    hypotheses_t hypotheses_;
    assembler_t assembler_;
    builder_t builder_;
    filter_t filter_;
    binder_t binder_;
    absorber_t absorber_;
    closer_t closer_;
    union_t union_;
    rules_t rules_;
    const expr* initial_;
};

#endif
