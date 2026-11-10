#ifndef LOGIC_FIT_RULES_MANIFEST_HPP
#define LOGIC_FIT_RULES_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "composition/poly_kernel_manifest.hpp"
#include "composition/pw_fit_rules_manifest.hpp"
#include "hypotheses/logic_bidecomposition_hypotheses.hpp"
#include "hypotheses/logic_cover_hypotheses.hpp"
#include "hypotheses/logic_decomposition_hypotheses.hpp"
#include "hypotheses/logic_parity_hypotheses.hpp"
#include "hypotheses/logic_primitive_hypotheses.hpp"
#include "hypotheses/pw_constant_hypotheses.hpp"
#include "infrastructure/abs_applying_evaluator.hpp"
#include "infrastructure/abs_binders.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/logic_cover_solver.hpp"
#include "infrastructure/logic_evaluator.hpp"
#include "infrastructure/logic_parity_solver.hpp"
#include "infrastructure/logic_table_reader.hpp"
#include "infrastructure/poly_sample_converter.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "rules/logic_formula_refitter.hpp"
#include "rules/logic_simplifier.hpp"
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting boolean functions to truth-table data over `arity` inputs, on a polynomial kernel (for its
// pool, literals and printer). A piecewise is a function of its inputs, so what replaces its catch-all is `lam(b)` or
// `lams(arity, b)`, the first input `#(arity-1)` and the last `#0` in b. The samples (inputs and outputs of 0s and 1s)
// become a piecewise with an open catch-all; the piecewise rules bind it to constants, parities
// (`logic_parity_hypotheses`), factored sums of products (`logic_cover_hypotheses`), primitives
// (`logic_primitive_hypotheses`), compositions of a primitive with an input (`logic_decomposition_hypotheses`) and
// compositions of any depth by bi-decomposition (`logic_bidecomposition_hypotheses`), and drop the branches they
// reproduce. The laws of Boolean algebra simplify any formula structurally, at any width
// (`logic_simplifier`), and any Boolean subformula is also refitted from its truth table by the leaf detectors and
// bi-decomposition (`logic_formula_refitter`, bounded in width), its holes as don't cares. `rules_` is what a search
// injects and `initial_` is where it starts.
struct logic_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    // ---- support: what the hypotheses and rules use ----
    struct converter_t : poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t> {
        using poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t>::poly_sample_converter;
    };
    struct evaluator_t : logic_evaluator<kernel_t::literals_t, kernel_t::literals_t> {
        using logic_evaluator<kernel_t::literals_t, kernel_t::literals_t>::logic_evaluator;
    };
    struct binders_t : abs_binders<kernel_t::literals_t> {
        using abs_binders<kernel_t::literals_t>::abs_binders;
    };
    struct applying_t : abs_applying_evaluator<binders_t, evaluator_t> {
        using abs_applying_evaluator<binders_t, evaluator_t>::abs_applying_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<applying_t, evaluator_t> {
        using pw_evaluator<applying_t, evaluator_t>::pw_evaluator;
    };
    struct function_maker_t : abs_function_maker<kernel_t::literals_t, expr_pool> {
        using abs_function_maker<kernel_t::literals_t, expr_pool>::abs_function_maker;
    };
    struct reader_t : logic_table_reader<kernel_t::literals_t> {
        using logic_table_reader<kernel_t::literals_t>::logic_table_reader;
    };

    // ---- hypotheses: add one's type here, then to `hypotheses_t` ----
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    struct parities_t : logic_parity_hypotheses<reader_t, logic_parity_solver, expr_pool> {
        using logic_parity_hypotheses<reader_t, logic_parity_solver, expr_pool>::logic_parity_hypotheses;
    };
    struct covers_t : logic_cover_hypotheses<reader_t, logic_cover_solver, expr_pool> {
        using logic_cover_hypotheses<reader_t, logic_cover_solver, expr_pool>::logic_cover_hypotheses;
    };
    struct primitives_t : logic_primitive_hypotheses<reader_t, expr_pool> {
        using logic_primitive_hypotheses<reader_t, expr_pool>::logic_primitive_hypotheses;
    };
    struct inner_t : pw_hypotheses_union<parities_t, primitives_t> {
        using pw_hypotheses_union<parities_t, primitives_t>::pw_hypotheses_union;
    };
    struct decompositions_t : logic_decomposition_hypotheses<reader_t, inner_t, kernel_t::literals_t, expr_pool> {
        using logic_decomposition_hypotheses<reader_t, inner_t, kernel_t::literals_t,
                                             expr_pool>::logic_decomposition_hypotheses;
    };
    struct leaves_t : pw_hypotheses_union<parities_t, covers_t, primitives_t> {
        using pw_hypotheses_union<parities_t, covers_t, primitives_t>::pw_hypotheses_union;
    };
    struct compositions_t : logic_bidecomposition_hypotheses<leaves_t, reader_t, evaluator_t, expr_size,
                                                             kernel_t::literals_t, expr_pool> {
        using logic_bidecomposition_hypotheses<leaves_t, reader_t, evaluator_t, expr_size, kernel_t::literals_t,
                                               expr_pool>::logic_bidecomposition_hypotheses;
    };
    using hypotheses_base_t = pw_hypotheses_union<
        constants_t,
        parities_t,
        covers_t,
        primitives_t,
        decompositions_t,
        compositions_t>;
    struct hypotheses_t : hypotheses_base_t {
        using hypotheses_base_t::hypotheses_base_t;
    };

    // ---- rules: add a rule's type here, then to `local_rules_t` (every position) or `root_rules_t` (the root) ----
    struct piecewise_t : pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t, kernel_t::literals_t> {
        using pw_fit_rules_manifest<hypotheses_t, pw_evaluator_t, function_maker_t,
                                    kernel_t::literals_t>::pw_fit_rules_manifest;
    };
    struct refits_t : pw_hypotheses_union<leaves_t, compositions_t> {
        using pw_hypotheses_union<leaves_t, compositions_t>::pw_hypotheses_union;
    };
    struct refitter_t : logic_formula_refitter<expr_size, evaluator_t, refits_t, kernel_t::literals_t,
                                               kernel_t::literals_t, expr_pool> {
        using logic_formula_refitter<expr_size, evaluator_t, refits_t, kernel_t::literals_t,
                                     kernel_t::literals_t, expr_pool>::logic_formula_refitter;
    };
    struct simplifier_t : logic_simplifier<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using logic_simplifier<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::logic_simplifier;
    };

    // ---- the rule lists ----
    using local_rules_base_t = proposer_union<
        piecewise_t::binder_t,
        piecewise_t::absorber_t,
        simplifier_t,
        refitter_t>;
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

    logic_fit_rules_manifest(kernel_t& kernel, size_t arity, const std::vector<poly_sample>& samples);

    pw_symbols pw_symbols_;
    abs_symbols abs_symbols_;
    logic_symbols logic_symbols_;
    uint32_t point_symbol_;
    std::vector<uint32_t> variables_;
    converter_t converter_;
    evaluator_t evaluator_;
    binders_t binders_;
    applying_t applying_;
    pw_evaluator_t pw_evaluator_;
    function_maker_t function_maker_;
    reader_t reader_;
    constants_t constants_;
    logic_parity_solver parity_solver_;
    parities_t parities_;
    logic_cover_solver cover_solver_;
    covers_t covers_;
    primitives_t primitives_;
    inner_t inner_;
    decompositions_t decompositions_;
    leaves_t leaves_;
    compositions_t compositions_;
    hypotheses_t hypotheses_;
    // rules
    piecewise_t piecewise_;
    simplifier_t simplifier_;
    refits_t refits_;
    refitter_t refitter_;

    // the rule lists
    local_rules_t local_rules_;
    root_rules_t root_rules_;
    subterms_t subterms_;
    rules_t rules_;
    const expr* initial_;
private:
    static std::vector<uint32_t> intern_inputs(kernel_t& kernel, size_t arity);
};

#endif
