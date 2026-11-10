#ifndef ARITH_FIT_RULES_MANIFEST_HPP
#define ARITH_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <vector>
#include "composition/arith_kernel_manifest.hpp"
#include "composition/pw_fit_rules_manifest.hpp"
#include "hypotheses/arith_linear_hypotheses.hpp"
#include "hypotheses/pw_constant_hypotheses.hpp"
#include "infrastructure/abs_applying_evaluator.hpp"
#include "infrastructure/abs_binders.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/arith_evaluator.hpp"
#include "infrastructure/arith_sample_converter.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "rules/arith_simplifier.hpp"
#include "value_objects/abs_symbols.hpp"
#include "value_objects/arith_sample.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting integer samples with arithmetic expressions, the smallest complete rule set: the samples
// become a piecewise with an open catch-all; the piecewise rules bind it to a constant or a line through the data and
// drop the branches it reproduces, and arithmetic simplification folds what remains, all at every position. `rules_`
// is what a search injects and `initial_` is where it starts.
struct arith_fit_rules_manifest {
    using kernel_t = arith_kernel_manifest;

    // ---- support: what the hypotheses and rules use ----
    struct function_maker_t : abs_function_maker<kernel_t::literals_t, expr_pool> {
        using abs_function_maker<kernel_t::literals_t, expr_pool>::abs_function_maker;
    };
    struct converter_t : arith_sample_converter<kernel_t::literals_t> {
        using arith_sample_converter<kernel_t::literals_t>::arith_sample_converter;
    };
    struct evaluator_t : arith_evaluator<kernel_t::literals_t, kernel_t::literals_t> {
        using arith_evaluator<kernel_t::literals_t, kernel_t::literals_t>::arith_evaluator;
    };
    struct binders_t : abs_binders<kernel_t::literals_t> {
        using abs_binders<kernel_t::literals_t>::abs_binders;
    };
    struct applying_t : abs_applying_evaluator<binders_t, evaluator_t> {
        using abs_applying_evaluator<binders_t, evaluator_t>::abs_applying_evaluator;
    };

    // ---- hypotheses: add one's type here, then to `hypotheses_t` ----
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    struct lines_t : arith_linear_hypotheses<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using arith_linear_hypotheses<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::arith_linear_hypotheses;
    };
    using hypotheses_base_t = pw_hypotheses_union<
        constants_t,
        lines_t>;
    struct hypotheses_t : hypotheses_base_t {
        using hypotheses_base_t::hypotheses_base_t;
    };

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct piecewise_t : pw_fit_rules_manifest<hypotheses_t, applying_t, function_maker_t, kernel_t::literals_t> {
        using pw_fit_rules_manifest<hypotheses_t, applying_t, function_maker_t,
                                    kernel_t::literals_t>::pw_fit_rules_manifest;
    };
    struct simplifier_t : arith_simplifier<kernel_t::literals_t, kernel_t::literals_t> {
        using arith_simplifier<kernel_t::literals_t, kernel_t::literals_t>::arith_simplifier;
    };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        piecewise_t::binder_t,
        piecewise_t::absorber_t,
        simplifier_t>;
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

    arith_fit_rules_manifest(kernel_t& kernel, const std::vector<arith_sample>& samples);

    pw_symbols pw_symbols_;
    abs_symbols abs_symbols_;
    uint32_t pow_symbol_;
    uint32_t point_symbol_;
    function_maker_t function_maker_;
    converter_t converter_;
    evaluator_t evaluator_;
    binders_t binders_;
    applying_t applying_;
    constants_t constants_;
    lines_t lines_;
    hypotheses_t hypotheses_;
    // rules
    piecewise_t piecewise_;
    simplifier_t simplifier_;
    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
    const expr* initial_;
};

#endif
