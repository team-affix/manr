#ifndef BEST_FIRST_SEARCH_HPP
#define BEST_FIRST_SEARCH_HPP

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <iterator>
#include <set>
#include <unordered_map>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/frontier_entry.hpp"

// Expands the cheapest expression on a bounded frontier, one per `next()`, until the frontier is empty.
// Expressions are interned, so an expression reached a second time (by any route) is dropped. When the frontier
// exceeds its capacity, its most expensive entry is evicted. The cheapest expression ever reached is kept.
template<typename IPropose, typename IGetCost>
struct best_first_search {
    best_first_search(IPropose& propose, IGetCost& get_cost, const expr* initial, size_t frontier_capacity);
    bool next();
    const expr* best() const;
    size_t best_cost() const;
    size_t expansions() const;
    std::vector<const expr*> derivation(const expr* reached) const;
private:
    void try_enqueue(const expr* e, const expr* parent);

    IPropose& propose_;
    IGetCost& get_cost_;
    size_t frontier_capacity_;
    std::set<frontier_entry> frontier_;
    std::unordered_map<const expr*, const expr*> parents_;
    std::vector<const expr*> candidates_;
    const expr* best_;
    size_t best_cost_;
    uint64_t enqueued_;
    size_t expansions_;
};

#define BEST_FIRST_SEARCH_TEMPLATE template<typename IPropose, typename IGetCost>
#define BEST_FIRST_SEARCH          best_first_search<IPropose, IGetCost>

BEST_FIRST_SEARCH_TEMPLATE BEST_FIRST_SEARCH::best_first_search(
    IPropose& propose, IGetCost& get_cost, const expr* initial, size_t frontier_capacity)
    : propose_(propose)
    , get_cost_(get_cost)
    , frontier_capacity_(frontier_capacity)
    , frontier_()
    , parents_()
    , candidates_()
    , best_(initial)
    , best_cost_(get_cost.cost(initial))
    , enqueued_(0)
    , expansions_(0) {
    try_enqueue(initial, nullptr);
}

BEST_FIRST_SEARCH_TEMPLATE bool BEST_FIRST_SEARCH::next() {
    if (frontier_.empty())
        return false;
    const frontier_entry cheapest = *frontier_.begin();
    frontier_.erase(frontier_.begin());
    candidates_.clear();
    propose_.propose(cheapest.expression, candidates_);
    for (const expr* candidate : candidates_)
        try_enqueue(candidate, cheapest.expression);
    ++expansions_;
    return true;
}

BEST_FIRST_SEARCH_TEMPLATE const expr* BEST_FIRST_SEARCH::best() const {
    return best_;
}

BEST_FIRST_SEARCH_TEMPLATE size_t BEST_FIRST_SEARCH::best_cost() const {
    return best_cost_;
}

BEST_FIRST_SEARCH_TEMPLATE size_t BEST_FIRST_SEARCH::expansions() const {
    return expansions_;
}

// The chain of rewrites from the initial expression to `reached`, initial first.
BEST_FIRST_SEARCH_TEMPLATE std::vector<const expr*> BEST_FIRST_SEARCH::derivation(const expr* reached) const {
    std::vector<const expr*> chain;
    for (const expr* step = reached; step != nullptr; step = parents_.at(step))
        chain.push_back(step);
    std::reverse(chain.begin(), chain.end());
    return chain;
}

BEST_FIRST_SEARCH_TEMPLATE void BEST_FIRST_SEARCH::try_enqueue(const expr* e, const expr* parent) {
    const bool reached_before = parents_.contains(e);
    if (reached_before)
        return;
    parents_.emplace(e, parent);
    const size_t cost = get_cost_.cost(e);
    if (cost < best_cost_) {
        best_ = e;
        best_cost_ = cost;
    }
    frontier_.insert(frontier_entry{.cost = cost, .order = enqueued_++, .expression = e});
    if (frontier_.size() > frontier_capacity_)
        frontier_.erase(std::prev(frontier_.end()));
}

#undef BEST_FIRST_SEARCH
#undef BEST_FIRST_SEARCH_TEMPLATE

#endif
