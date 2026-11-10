#ifndef LOGIC_FIT_RULES_MANIFEST_HPP
#define LOGIC_FIT_RULES_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/abs_applying_evaluator.hpp"
#include "infrastructure/abs_binders.hpp"
#include "infrastructure/abs_closing_hypotheses.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/arith_hole_solver.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/logic_cover_hypotheses.hpp"
#include "infrastructure/logic_cover_solver.hpp"
#include "infrastructure/logic_decomposition_hypotheses.hpp"
#include "infrastructure/logic_evaluator.hpp"
#include "infrastructure/logic_formula_refitter.hpp"
#include "infrastructure/logic_parity_hypotheses.hpp"
#include "infrastructure/logic_parity_solver.hpp"
#include "infrastructure/logic_primitive_hypotheses.hpp"
#include "infrastructure/logic_simplifier.hpp"
#include "infrastructure/logic_table_reader.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_sample_converter.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_assembler.hpp"
#include "infrastructure/pw_branch_absorber.hpp"
#include "infrastructure/pw_branch_filter.hpp"
#include "infrastructure/pw_builder.hpp"
#include "infrastructure/pw_constant_hypotheses.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_extension_binder.hpp"
#include "infrastructure/pw_hole_matcher.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
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
// (`logic_primitive_hypotheses`) and compositions of a primitive with an input (`logic_decomposition_hypotheses`),
// and drop the branches they reproduce. The laws of Boolean algebra simplify any formula structurally, at any width
// (`logic_simplifier`), and any Boolean subformula is also refitted from its truth table by the cheaper detectors
// (`logic_formula_refitter`, bounded in width), its holes as don't cares. `rules_` is what a search injects and
// `initial_` is where it starts.
struct logic_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
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
    struct hypotheses_t
        : pw_hypotheses_union<constants_t, parities_t, covers_t, primitives_t, decompositions_t> {
        using pw_hypotheses_union<constants_t, parities_t, covers_t, primitives_t,
                                  decompositions_t>::pw_hypotheses_union;
    };
    struct refits_t : pw_hypotheses_union<parities_t, covers_t, primitives_t> {
        using pw_hypotheses_union<parities_t, covers_t, primitives_t>::pw_hypotheses_union;
    };
    struct refitter_t : logic_formula_refitter<expr_size, evaluator_t, refits_t, kernel_t::literals_t,
                                               kernel_t::literals_t, expr_pool> {
        using logic_formula_refitter<expr_size, evaluator_t, refits_t, kernel_t::literals_t,
                                     kernel_t::literals_t, expr_pool>::logic_formula_refitter;
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
    struct simplifier_t : logic_simplifier<kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using logic_simplifier<kernel_t::literals_t, kernel_t::literals_t, expr_pool>::logic_simplifier;
    };
    struct union_t : proposer_union<binder_t, absorber_t, simplifier_t, refitter_t> {
        using proposer_union<binder_t, absorber_t, simplifier_t, refitter_t>::proposer_union;
    };
    struct rules_t : subterm_proposer<union_t, expr_pool> {
        using subterm_proposer<union_t, expr_pool>::subterm_proposer;
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
    hypotheses_t hypotheses_;
    closing_t closing_;
    assembler_t assembler_;
    builder_t builder_;
    solver_t solver_;
    matcher_t matcher_;
    filter_t filter_;
    binder_t binder_;
    absorber_t absorber_;
    simplifier_t simplifier_;
    refits_t refits_;
    refitter_t refitter_;
    union_t union_;
    rules_t rules_;
    const expr* initial_;
private:
    static std::vector<uint32_t> intern_inputs(kernel_t& kernel, size_t arity);
};

#endif
