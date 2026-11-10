#include "composition/arith_fit_mcts_runtime.hpp"
#include <sstream>

arith_fit_mcts_runtime::arith_fit_mcts_runtime(
    const std::vector<arith_sample>& samples,
    size_t probing_depth,
    size_t max_simulations,
    double exploration_constant,
    uint32_t seed)
    : manifest_(samples, probing_depth, max_simulations, exploration_constant, seed) {}

bool arith_fit_mcts_runtime::next() {
    return manifest_.search_.next();
}

std::string arith_fit_mcts_runtime::initial() const {
    return print(manifest_.rule_set_.initial_);
}

std::string arith_fit_mcts_runtime::best() const {
    return print(manifest_.search_.best());
}

size_t arith_fit_mcts_runtime::best_cost() const {
    return manifest_.search_.best_cost();
}

size_t arith_fit_mcts_runtime::simulations() const {
    return manifest_.search_.simulations();
}

std::vector<std::string> arith_fit_mcts_runtime::derivation() const {
    std::vector<std::string> steps;
    for (const expr* step : manifest_.search_.derivation())
        steps.push_back(print(step));
    return steps;
}

std::string arith_fit_mcts_runtime::print(const expr* e) const {
    std::ostringstream os;
    manifest_.kernel_.printer_.print(os, e);
    return os.str();
}
