#ifndef LOGIC_FIT_MCTS_MANIFEST_HPP
#define LOGIC_FIT_MCTS_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "infrastructure/detection_memo.hpp"
#include "infrastructure/expr_size.hpp"
#include "infrastructure/mcts_path_walker.hpp"
#include "infrastructure/mcts_search.hpp"
#include "infrastructure/uct_value_manifest.hpp"
#include "infrastructure/value_table.hpp"
#include "infrastructure/visits_table.hpp"
#include "infrastructure/logic_fit_rules_manifest.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "value_objects/poly_limits.hpp"
#include "value_objects/poly_sample.hpp"

// `logic_fit_manifest` with Monte Carlo tree search in place of best-first: the same rules, searched by probing
// simulations that are credited with the smallest expression they reach.
struct logic_fit_mcts_manifest {
    using rule_set_t = logic_fit_rules_manifest;
    struct visits_t : monte_carlo::visits_table<uint64_t, std::unordered_map> {};
    struct values_t : monte_carlo::value_table<uint64_t, double, std::unordered_map> {};
    using uct_base_t = monte_carlo::uct_value_manifest<
        uint64_t, size_t, double, visits_t, visits_t, values_t, values_t, mcts_path_walker,
        std::vector<size_t>, std::vector<size_t>, std::mt19937>;
    struct uct_t : uct_base_t {
        using uct_base_t::uct_base_t;
    };
    struct memo_t : detection_memo<rule_set_t::rules_t> {
        using detection_memo<rule_set_t::rules_t>::detection_memo;
    };
    using search_base_t = mcts_search<memo_t, expr_size, uct_base_t::chooser_t, uct_base_t::delta_t,
                                      uct_base_t::terminator_t>;
    struct search_t : search_base_t {
        using search_base_t::search_base_t;
    };

    logic_fit_mcts_manifest(const std::vector<std::string>& variables,
                            const std::vector<poly_sample>& samples,
                            const poly_limits& limits,
                            size_t probing_depth,
                            size_t max_simulations,
                            double exploration_constant,
                            uint32_t seed);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    visits_t visits_;
    values_t values_;
    std::mt19937 rng_;
    uct_t uct_;
    memo_t memo_;
    search_t search_;
};

#endif
