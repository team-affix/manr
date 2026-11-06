#ifndef POLY_BASELINE_MCTS_MANIFEST_HPP
#define POLY_BASELINE_MCTS_MANIFEST_HPP

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>
#include "infrastructure/expr_size.hpp"
#include "infrastructure/mcts_path_walker.hpp"
#include "infrastructure/mcts_search.hpp"
#include "infrastructure/uct_value_manifest.hpp"
#include "infrastructure/value_table.hpp"
#include "infrastructure/visits_table.hpp"
#include "infrastructure/poly_kernel_manifest.hpp"
#include "infrastructure/poly_local_rules_manifest.hpp"
#include "value_objects/expr.hpp"

// Baseline: low-level rules only, under Monte Carlo tree search. The problem is infix polynomial text.
struct poly_baseline_mcts_manifest {
    using rule_set_t = poly_local_rules_manifest;
    struct visits_t : monte_carlo::visits_table<uint64_t, std::unordered_map> {};
    struct values_t : monte_carlo::value_table<uint64_t, double, std::unordered_map> {};
    using uct_base_t = monte_carlo::uct_value_manifest<
        uint64_t, const expr*, double, visits_t, visits_t, values_t, values_t, mcts_path_walker,
        std::vector<const expr*>, std::vector<const expr*>, std::mt19937>;
    struct uct_t : uct_base_t {
        using uct_base_t::uct_base_t;
    };
    using search_base_t = mcts_search<rule_set_t::rules_t, expr_size, uct_base_t::chooser_t, uct_base_t::delta_t,
                                      uct_base_t::terminator_t>;
    struct search_t : search_base_t {
        using search_base_t::search_base_t;
    };

    poly_baseline_mcts_manifest(const std::string& problem,
        size_t probing_depth,
        size_t max_simulations,
        double exploration_constant,
        uint32_t seed);

    poly_kernel_manifest kernel_;
    rule_set_t rule_set_;
    const expr* initial_;
    visits_t visits_;
    values_t values_;
    std::mt19937 rng_;
    uct_t uct_;
    search_t search_;
};

#endif
