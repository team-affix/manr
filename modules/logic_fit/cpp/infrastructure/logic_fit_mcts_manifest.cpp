#include "infrastructure/logic_fit_mcts_manifest.hpp"

logic_fit_mcts_manifest::logic_fit_mcts_manifest(
    size_t arity,
    const std::vector<poly_sample>& samples,
    const poly_limits& limits,
    size_t probing_depth,
    size_t max_simulations,
    double exploration_constant,
    uint32_t seed)
    : kernel_(limits)
    , rule_set_(kernel_, arity, samples)
    , visits_()
    , values_()
    , rng_(seed)
    , uct_(visits_, visits_, values_, values_, rng_, exploration_constant, 0)
    , memo_(rule_set_.rules_)
    , search_(memo_, kernel_.sizes_, uct_.chooser, uct_.delta, uct_.terminator, rule_set_.initial_,
              probing_depth, max_simulations) {}
