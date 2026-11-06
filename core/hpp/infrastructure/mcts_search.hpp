#ifndef MCTS_SEARCH_HPP
#define MCTS_SEARCH_HPP

#include <cstddef>
#include <vector>
#include "value_objects/expr.hpp"

// Monte Carlo tree search over rewrites, for reductions that only pay off several rewrites later.
//
// Each `next()` is one simulation from the initial expression: up to `probing_depth` rewrites, each chosen by the
// injected chooser (UCT inside the tree, uniformly at random past its frontier), stopping early at an expression
// with no proposals. The simulation is rewarded by the smallest cost it reached anywhere along the way, not the
// cost it ended on, because any prefix of a derivation is itself a derivation:
//
//     reward = (initial cost - smallest cost reached) / initial cost
//
// so an expansion that leads to a big collapse within the probing depth is credited with the collapse.
// The cheapest expression any simulation reached is kept, with the rewrites that led to it.
template<typename IPropose, typename IGetCost, typename IChoose, typename ISetReward, typename ITerminate>
struct mcts_search {
    mcts_search(IPropose& propose,
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
    IPropose& propose_;
    IGetCost& get_cost_;
    IChoose& choose_;
    ISetReward& set_reward_;
    ITerminate& terminate_;
    const expr* initial_;
    size_t initial_cost_;
    size_t probing_depth_;
    size_t max_simulations_;
    std::vector<const expr*> candidates_;
    std::vector<const expr*> path_;
    std::vector<const expr*> derivation_;
    size_t best_cost_;
    size_t simulations_;
};

#define MCTS_SEARCH_TEMPLATE \
    template<typename IPropose, typename IGetCost, typename IChoose, typename ISetReward, typename ITerminate>
#define MCTS_SEARCH mcts_search<IPropose, IGetCost, IChoose, ISetReward, ITerminate>

MCTS_SEARCH_TEMPLATE MCTS_SEARCH::mcts_search(
    IPropose& propose,
    IGetCost& get_cost,
    IChoose& choose,
    ISetReward& set_reward,
    ITerminate& terminate,
    const expr* initial,
    size_t probing_depth,
    size_t max_simulations)
    : propose_(propose)
    , get_cost_(get_cost)
    , choose_(choose)
    , set_reward_(set_reward)
    , terminate_(terminate)
    , initial_(initial)
    , initial_cost_(get_cost.cost(initial))
    , probing_depth_(probing_depth)
    , max_simulations_(max_simulations)
    , candidates_()
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
        candidates_.clear();
        propose_.propose(path_.back(), candidates_);
        if (candidates_.empty())
            break;
        path_.push_back(choose_.choose(candidates_, candidates_));
        const size_t cost = get_cost_.cost(path_.back());
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
