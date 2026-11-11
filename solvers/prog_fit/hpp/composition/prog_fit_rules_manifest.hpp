#ifndef PROG_FIT_RULES_MANIFEST_HPP
#define PROG_FIT_RULES_MANIFEST_HPP

#include <cstdint>
#include <vector>
#include "composition/poly_kernel_manifest.hpp"
#include "composition/poly_rules_manifest.hpp"
#include "composition/pw_fit_rules_manifest.hpp"
#include "hypotheses/prog_enumerative_hypotheses.hpp"
#include "hypotheses/pw_constant_hypotheses.hpp"
#include "infrastructure/abs_function_maker.hpp"
#include "infrastructure/abs_indices.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/prog_interpreter.hpp"
#include "infrastructure/prog_value_parser.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_evaluator.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "infrastructure/symbol_table.hpp"
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/prog_sample.hpp"
#include "value_objects/prog_symbols.hpp"
#include "value_objects/pw_symbols.hpp"

// The rules for fitting generic programs to data of any shape (integers, lists, nested lists, trees, records, tuples
// of inputs), on a polynomial kernel: the samples, parsed from text (`prog_value_parser`), become a piecewise with an
// open catch-all; the piecewise rules bind it to constants and to the smallest program the enumerator finds
// (`prog_enumerative_hypotheses`), evaluated by the program interpreter, and the polynomial rules compress bodies.
struct prog_fit_rules_manifest {
    using kernel_t = poly_kernel_manifest;
    // ---- support: what the hypotheses and rules use ----
    struct indices_t : abs_indices<symbol_table, expr_pool> {
        using abs_indices<symbol_table, expr_pool>::abs_indices;
    };
    struct interpreter_t : prog_interpreter<indices_t, kernel_t::literals_t, kernel_t::literals_t, expr_pool> {
        using prog_interpreter<indices_t, kernel_t::literals_t, kernel_t::literals_t, expr_pool>::prog_interpreter;
    };
    struct pw_evaluator_t : pw_evaluator<interpreter_t, interpreter_t> {
        using pw_evaluator<interpreter_t, interpreter_t>::pw_evaluator;
    };
    struct function_maker_t : abs_function_maker<kernel_t::literals_t, expr_pool> {
        using abs_function_maker<kernel_t::literals_t, expr_pool>::abs_function_maker;
    };
    struct parser_t : prog_value_parser<kernel_t::literals_t, expr_pool, symbol_table, expr_pool> {
        using prog_value_parser<kernel_t::literals_t, expr_pool, symbol_table, expr_pool>::prog_value_parser;
    };
    // ---- hypotheses: add one's type here, then to `hypotheses_t` ----
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    using programs_base_t = prog_enumerative_hypotheses<interpreter_t, kernel_t::literals_t, kernel_t::literals_t,
                                                        indices_t, expr_pool>;
    struct programs_t : programs_base_t {
        using programs_base_t::programs_base_t;
    };
    using hypotheses_base_t = pw_hypotheses_union<
        constants_t,
        programs_t>;
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
    using root_rules_base_t = proposer_union<>;
    struct root_rules_t : root_rules_base_t {
        using root_rules_base_t::root_rules_base_t;
    };
    struct subterms_t : subterm_proposer<local_rules_t, expr_pool> {
        using subterm_proposer<local_rules_t, expr_pool>::subterm_proposer;
    };
    struct rules_t : proposer_union<subterms_t, root_rules_t> {
        using proposer_union<subterms_t, root_rules_t>::proposer_union;
    };

    prog_fit_rules_manifest(kernel_t& kernel, const std::vector<prog_sample>& samples);

    poly_rules_manifest poly_rules_;
    pw_symbols pw_symbols_;
    abs_symbols abs_symbols_;
    prog_symbols prog_symbols_;
    // support
    indices_t indices_;
    interpreter_t interpreter_;
    pw_evaluator_t pw_evaluator_;
    function_maker_t function_maker_;
    parser_t parser_;
    std::vector<pw_data_point> points_;
    // hypotheses
    constants_t constants_;
    programs_t programs_;
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
    static prog_symbols intern_symbols(kernel_t& kernel);
    static std::vector<pw_data_point> parse_samples(parser_t& parser, const std::vector<prog_sample>& samples);
};

#endif
