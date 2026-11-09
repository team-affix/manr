#ifndef LOGIC_FIT_RULES_MANIFEST_HPP
#define LOGIC_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/logic_cover_hypotheses.hpp"
#include "infrastructure/logic_cover_solver.hpp"
#include "infrastructure/logic_evaluator.hpp"
#include "infrastructure/logic_parity_hypotheses.hpp"
#include "infrastructure/logic_parity_solver.hpp"
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
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/logic_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting boolean functions to truth-table data over named variables, on a polynomial kernel (for its
// pool, literals and printer): the samples (inputs and outputs of 0s and 1s) become a piecewise with an open
// catch-all; the piecewise rules bind it to constants, parities (`logic_parity_hypotheses`) and factored sums of
// products (`logic_cover_hypotheses`), and drop the branches they reproduce. `rules_` is what a search injects and
// `initial_` is where it starts.
struct logic_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    struct converter_t : poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t> {
        using poly_sample_converter<kernel_t::literals_t, expr_pool, kernel_t::literals_t>::poly_sample_converter;
    };
    struct evaluator_t : logic_evaluator<kernel_t::literals_t, kernel_t::literals_t> {
        using logic_evaluator<kernel_t::literals_t, kernel_t::literals_t>::logic_evaluator;
    };
    struct pw_evaluator_t : pw_evaluator<evaluator_t, evaluator_t> {
        using pw_evaluator<evaluator_t, evaluator_t>::pw_evaluator;
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
    struct hypotheses_t : pw_hypotheses_union<constants_t, parities_t, covers_t> {
        using pw_hypotheses_union<constants_t, parities_t, covers_t>::pw_hypotheses_union;
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
    struct union_t : proposer_union<binder_t, absorber_t> {
        using proposer_union<binder_t, absorber_t>::proposer_union;
    };
    struct rules_t : subterm_proposer<union_t, expr_pool> {
        using subterm_proposer<union_t, expr_pool>::subterm_proposer;
    };

    logic_fit_rules_manifest(kernel_t& kernel,
                             const std::vector<std::string>& variables,
                             const std::vector<poly_sample>& samples);

    pw_symbols pw_symbols_;
    logic_symbols logic_symbols_;
    uint32_t point_symbol_;
    std::vector<uint32_t> variables_;
    converter_t converter_;
    evaluator_t evaluator_;
    pw_evaluator_t pw_evaluator_;
    reader_t reader_;
    constants_t constants_;
    logic_parity_solver parity_solver_;
    parities_t parities_;
    logic_cover_solver cover_solver_;
    covers_t covers_;
    hypotheses_t hypotheses_;
    assembler_t assembler_;
    builder_t builder_;
    filter_t filter_;
    binder_t binder_;
    absorber_t absorber_;
    union_t union_;
    rules_t rules_;
    const expr* initial_;
private:
    static std::vector<uint32_t> intern_variables(kernel_t& kernel, const std::vector<std::string>& variables);
};

#endif
