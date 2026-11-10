#include "composition/poly_baseline_mcts_manifest.hpp"

poly_baseline_mcts_manifest::poly_baseline_mcts_manifest(
    const std::string& problem,
    const poly_limits& limits,
    size_t probing_depth,
    size_t max_simulations,
    double exploration_constant,
    uint32_t seed)
    : kernel_(limits)
    , rule_set_(kernel_)
    , initial_(kernel_.parser_.parse(problem))
    , visits_()
    , values_()
    , rng_(seed)
    , uct_(visits_, visits_, values_, values_, rng_, exploration_constant, 0)
    , memo_(rule_set_.rules_)
    , search_(memo_, kernel_.sizes_, uct_.chooser, uct_.delta, uct_.terminator, initial_, probing_depth,
              max_simulations) {}
