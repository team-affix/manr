#ifndef MCTS_SEARCH_HPP
#define MCTS_SEARCH_HPP

#include <cstddef>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/rewrite.hpp"
#include "debug_assert.hpp"

// Monte Carlo tree search over rewrites, for reductions that only pay off several rewrites later.
//
// Each `next()` is one simulation from the initial expression: up to `probing_depth` steps. At each step the rewrites
// of the current expression are detected, the injected chooser picks one by index (UCT inside the tree, uniformly at
// random past its frontier), and only that one is performed. A simulation stops early at an expression with no
// rewrites. The simulation is rewarded by the smallest cost it reached anywhere along the way, not the
// cost it ended on, because any prefix of a derivation is itself a derivation:
//
//     reward = (initial cost - smallest cost reached) / initial cost
//
// so an expansion that leads to a big collapse within the probing depth is credited with the collapse.
// The cheapest expression any simulation reached is kept, with the rewrites that led to it.
template<typename IDetect, typename IGetCost, typename IChoose, typename ISetReward, typename ITerminate>
struct mcts_search {
    mcts_search(IDetect& detect,
                IGetCost& get_cost,
                IChoose& choose,
                ISetReward& set_reward,
                ITerminate& terminate,
                const expr* initial,
                size_t probing_depth,
                size_t max_simulations);
    bool next();
    const expr* best() const;
    size_t best_cost() const;
    size_t simulations() const;
    const std::vector<const expr*>& derivation() const;
private:
    IDetect& detect_;
    IGetCost& get_cost_;
    IChoose& choose_;
    ISetReward& set_reward_;
    ITerminate& terminate_;
    const expr* initial_;
    size_t initial_cost_;
    size_t probing_depth_;
    size_t max_simulations_;
    std::vector<rewrite> detected_;
    std::vector<size_t> choices_;
    std::vector<const expr*> path_;
    std::vector<const expr*> derivation_;
    size_t best_cost_;
    size_t simulations_;
};

#define MCTS_SEARCH_TEMPLATE \
    template<typename IDetect, typename IGetCost, typename IChoose, typename ISetReward, typename ITerminate>
#define MCTS_SEARCH mcts_search<IDetect, IGetCost, IChoose, ISetReward, ITerminate>

MCTS_SEARCH_TEMPLATE MCTS_SEARCH::mcts_search(
    IDetect& detect,
    IGetCost& get_cost,
    IChoose& choose,
    ISetReward& set_reward,
    ITerminate& terminate,
    const expr* initial,
    size_t probing_depth,
    size_t max_simulations)
    : detect_(detect)
    , get_cost_(get_cost)
    , choose_(choose)
    , set_reward_(set_reward)
    , terminate_(terminate)
    , initial_(initial)
    , initial_cost_(get_cost.cost(initial))
    , probing_depth_(probing_depth)
    , max_simulations_(max_simulations)
    , detected_()
    , choices_()
    , path_()
    , derivation_{initial}
    , best_cost_(initial_cost_)
    , simulations_(0) {}

MCTS_SEARCH_TEMPLATE bool MCTS_SEARCH::next() {
    if (simulations_ == max_simulations_)
        return false;
    path_.assign(1, initial_);
    size_t smallest_cost = initial_cost_;
    for (size_t step = 0; step < probing_depth_; ++step) {
        detected_.clear();
        detect_.detect(path_.back(), detected_);
        if (detected_.empty())
            break;
        choices_.resize(detected_.size());
        for (size_t index = 0; index < choices_.size(); ++index)
            choices_[index] = index;
        const expr* result = detected_[choose_.choose(choices_, choices_)]();
        DEBUG_ASSERT(result != nullptr);
        path_.push_back(result);
        const size_t cost = get_cost_.cost(result);
        if (cost < smallest_cost)
            smallest_cost = cost;
        if (cost >= best_cost_)
            continue;
        best_cost_ = cost;
        derivation_ = path_;
    }
    set_reward_.set_value(static_cast<double>(initial_cost_ - smallest_cost) / static_cast<double>(initial_cost_));
    terminate_.terminate();
    ++simulations_;
    return true;
}

MCTS_SEARCH_TEMPLATE const expr* MCTS_SEARCH::best() const {
    return derivation_.back();
}

MCTS_SEARCH_TEMPLATE size_t MCTS_SEARCH::best_cost() const {
    return best_cost_;
}

MCTS_SEARCH_TEMPLATE size_t MCTS_SEARCH::simulations() const {
    return simulations_;
}

// The rewrites from the initial expression to the best, initial first.
MCTS_SEARCH_TEMPLATE const std::vector<const expr*>& MCTS_SEARCH::derivation() const {
    return derivation_;
}

#undef MCTS_SEARCH
#undef MCTS_SEARCH_TEMPLATE

#endif
