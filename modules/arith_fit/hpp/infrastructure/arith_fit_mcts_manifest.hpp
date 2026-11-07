#ifndef ARITH_FIT_MCTS_MANIFEST_HPP
#define ARITH_FIT_MCTS_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <vector>
#include "infrastructure/arith_evaluator.hpp"
#include "infrastructure/arith_linear_hypotheses.hpp"
#include "infrastructure/arith_literals.hpp"
#include "infrastructure/arith_sample_converter.hpp"
#include "infrastructure/arith_simplifier.hpp"
#include "infrastructure/expr_pool.hpp"
#include "infrastructure/expr_printer.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/mcts_path_walker.hpp"
#include "infrastructure/mcts_search.hpp"
#include "infrastructure/proposer_union.hpp"
#include "infrastructure/pw_assembler.hpp"
#include "infrastructure/pw_branch_absorber.hpp"
#include "infrastructure/pw_branch_filter.hpp"
#include "infrastructure/pw_builder.hpp"
#include "infrastructure/pw_constant_hypotheses.hpp"
#include "infrastructure/pw_extension_binder.hpp"
#include "infrastructure/pw_hypotheses_union.hpp"
#include "infrastructure/subterm_proposer.hpp"
#include "infrastructure/symbol_table.hpp"
#include "infrastructure/uct_value_manifest.hpp"
#include "infrastructure/value_table.hpp"
#include "infrastructure/visits_table.hpp"
#include "value_objects/arith_sample.hpp"
#include "value_objects/arith_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/pw_symbols.hpp"

// `arith_fit_manifest` with Monte Carlo tree search in place of best-first: the same rules, searched by probing
// simulations that are credited with the smallest expression they reach.
struct arith_fit_mcts_manifest {
    struct literals_t : arith_literals<symbol_table, expr_pool> {
        using arith_literals<symbol_table, expr_pool>::arith_literals;
    };
    struct converter_t : arith_sample_converter<literals_t> {
        using arith_sample_converter<literals_t>::arith_sample_converter;
    };
    struct evaluator_t : arith_evaluator<literals_t, literals_t> {
        using arith_evaluator<literals_t, literals_t>::arith_evaluator;
    };
    struct assembler_t : pw_assembler<expr_pool> {
        using pw_assembler<expr_pool>::pw_assembler;
    };
    struct builder_t : pw_builder<expr_pool, expr_pool, assembler_t> {
        using pw_builder<expr_pool, expr_pool, assembler_t>::pw_builder;
    };
    struct filter_t : pw_branch_filter<evaluator_t> {
        using pw_branch_filter<evaluator_t>::pw_branch_filter;
    };
    struct lines_t : arith_linear_hypotheses<literals_t, literals_t, expr_pool> {
        using arith_linear_hypotheses<literals_t, literals_t, expr_pool>::arith_linear_hypotheses;
    };
    struct constants_t : pw_constant_hypotheses<expr_size> {
        using pw_constant_hypotheses<expr_size>::pw_constant_hypotheses;
    };
    struct hypotheses_t : pw_hypotheses_union<constants_t, lines_t> {
        using pw_hypotheses_union<constants_t, lines_t>::pw_hypotheses_union;
    };
    struct binder_t : pw_extension_binder<hypotheses_t, filter_t, assembler_t> {
        using pw_extension_binder<hypotheses_t, filter_t, assembler_t>::pw_extension_binder;
    };
    struct absorber_t : pw_branch_absorber<filter_t, assembler_t> {
        using pw_branch_absorber<filter_t, assembler_t>::pw_branch_absorber;
    };
    struct simplifier_t : arith_simplifier<literals_t, literals_t> {
        using arith_simplifier<literals_t, literals_t>::arith_simplifier;
    };
    struct local_rules_t : proposer_union<binder_t, absorber_t, simplifier_t> {
        using proposer_union<binder_t, absorber_t, simplifier_t>::proposer_union;
    };
    struct rules_t : subterm_proposer<local_rules_t, expr_pool> {
        using subterm_proposer<local_rules_t, expr_pool>::subterm_proposer;
    };
    struct visits_t : monte_carlo::visits_table<uint64_t, std::unordered_map> {};
    struct values_t : monte_carlo::value_table<uint64_t, double, std::unordered_map> {};
    using uct_base_t = monte_carlo::uct_value_manifest<
        uint64_t, size_t, double, visits_t, visits_t, values_t, values_t, mcts_path_walker,
        std::vector<size_t>, std::vector<size_t>, std::mt19937>;
    struct uct_t : uct_base_t {
        using uct_base_t::uct_base_t;
    };
    using search_base_t =
        mcts_search<rules_t, expr_size, uct_base_t::chooser_t, uct_base_t::delta_t, uct_base_t::terminator_t>;
    struct search_t : search_base_t {
        using search_base_t::search_base_t;
    };
    struct printer_t : expr_printer<symbol_table> {
        using expr_printer<symbol_table>::expr_printer;
    };

    arith_fit_mcts_manifest(const std::vector<arith_sample>& samples,
                            size_t probing_depth,
                            size_t max_simulations,
                            double exploration_constant,
                            uint32_t seed);

    symbol_table symbols_;
    expr_pool exprs_;
    expr_size sizes_;
    arith_symbols arith_symbols_;
    pw_symbols pw_symbols_;
    literals_t literals_;
    converter_t converter_;
    evaluator_t evaluator_;
    assembler_t assembler_;
    builder_t builder_;
    filter_t filter_;
    constants_t constants_;
    lines_t lines_;
    hypotheses_t hypotheses_;
    binder_t binder_;
    absorber_t absorber_;
    simplifier_t simplifier_;
    local_rules_t local_rules_;
    rules_t rules_;
    const expr* initial_;
    visits_t visits_;
    values_t values_;
    std::mt19937 rng_;
    uct_t uct_;
    search_t search_;
    printer_t printer_;
};

#endif
