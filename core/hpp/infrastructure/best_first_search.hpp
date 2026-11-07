#ifndef BEST_FIRST_SEARCH_HPP
#define BEST_FIRST_SEARCH_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/frontier_entry.hpp"
#include "value_objects/rewrite.hpp"
#include "debug_assert.hpp"

// Best-first search over expressions. The frontier holds reached expressions not yet expanded, keyed by their own
// cost. Each `next()` expands the cheapest: detects its rewrites (the expensive part) and performs all of them, so
// every result is costed before anything is ordered by it. A result reached before (expressions are interned) is not
// queued again. When the frontier exceeds its capacity, the most expensive expression is evicted. The cheapest
// expression ever reached is kept.
//
// Detection happens only for expanded expressions; performing is the certain, cheap remainder. Ordering by the
// source's cost instead (performing one rewrite per step) lets a cheap result's descendants starve its siblings.
template<typename IDetect, typename IGetCost>
struct best_first_search {
    best_first_search(IDetect& detect, IGetCost& get_cost, const expr* initial, size_t frontier_capacity);
    bool next();
    const expr* best() const;
    size_t best_cost() const;
    size_t rewrites() const;
    std::vector<const expr*> derivation(const expr* reached) const;
private:
    IDetect& detect_;
    IGetCost& get_cost_;
    size_t frontier_capacity_;
    std::map<frontier_entry, const expr*> frontier_;
    std::unordered_map<const expr*, const expr*> parents_;
    std::vector<rewrite> detected_;
    const expr* best_;
    size_t best_cost_;
    uint64_t enqueued_;
    size_t rewrites_;
};

#define BEST_FIRST_SEARCH_TEMPLATE template<typename IDetect, typename IGetCost>
#define BEST_FIRST_SEARCH          best_first_search<IDetect, IGetCost>

BEST_FIRST_SEARCH_TEMPLATE BEST_FIRST_SEARCH::best_first_search(
    IDetect& detect, IGetCost& get_cost, const expr* initial, size_t frontier_capacity)
    : detect_(detect)
    , get_cost_(get_cost)
    , frontier_capacity_(frontier_capacity)
    , frontier_{{frontier_entry{.cost = get_cost.cost(initial), .order = 0}, initial}}
    , parents_{{initial, nullptr}}
    , detected_()
    , best_(initial)
    , best_cost_(get_cost.cost(initial))
    , enqueued_(1)
    , rewrites_(0) {}

BEST_FIRST_SEARCH_TEMPLATE bool BEST_FIRST_SEARCH::next() {
    if (frontier_.empty())
        return false;
    const expr* expanded = frontier_.begin()->second;
    frontier_.erase(frontier_.begin());
    detected_.clear();
    detect_.detect(expanded, detected_);
    for (const rewrite& detected : detected_) {
        ++rewrites_;
        const expr* result = detected();
        DEBUG_ASSERT(result != nullptr);
        if (parents_.contains(result))
            continue;
        parents_.emplace(result, expanded);
        const size_t cost = get_cost_.cost(result);
        if (cost < best_cost_) {
            best_ = result;
            best_cost_ = cost;
        }
        frontier_.emplace(frontier_entry{.cost = cost, .order = enqueued_++}, result);
        if (frontier_.size() > frontier_capacity_)
            frontier_.erase(std::prev(frontier_.end()));
    }
    return true;
}

BEST_FIRST_SEARCH_TEMPLATE const expr* BEST_FIRST_SEARCH::best() const {
    return best_;
}

BEST_FIRST_SEARCH_TEMPLATE size_t BEST_FIRST_SEARCH::best_cost() const {
    return best_cost_;
}

// Rewrites performed so far, including ones that reached a known expression.
BEST_FIRST_SEARCH_TEMPLATE size_t BEST_FIRST_SEARCH::rewrites() const {
    return rewrites_;
}

// The chain of rewrites from the initial expression to `reached`, initial first.
BEST_FIRST_SEARCH_TEMPLATE std::vector<const expr*> BEST_FIRST_SEARCH::derivation(const expr* reached) const {
    std::vector<const expr*> chain;
    for (const expr* step = reached; step != nullptr; step = parents_.at(step))
        chain.push_back(step);
    std::reverse(chain.begin(), chain.end());
    return chain;
}

#undef BEST_FIRST_SEARCH
#undef BEST_FIRST_SEARCH_TEMPLATE

#endif
