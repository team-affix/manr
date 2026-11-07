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

// Lazy best-first search. The frontier holds detected rewrites, not results, keyed by the cost of the expression they
// were detected on. Each `next()` performs the rewrite whose source is cheapest; only then is its result built and
// costed. A new result has its own rewrites detected and added. A result reached before (expressions are interned)
// adds nothing. When the frontier exceeds its capacity, the
// rewrite with the most expensive source is evicted. The cheapest expression ever reached is kept.
template<typename IDetect, typename IGetCost>
struct best_first_search {
    best_first_search(IDetect& detect, IGetCost& get_cost, const expr* initial, size_t frontier_capacity);
    bool next();
    const expr* best() const;
    size_t best_cost() const;
    size_t rewrites() const;
    std::vector<const expr*> derivation(const expr* reached) const;
private:
    struct pending {
        rewrite apply;
        const expr* source;
    };

    void enqueue_rewrites(const expr* e, size_t cost);

    IDetect& detect_;
    IGetCost& get_cost_;
    size_t frontier_capacity_;
    std::map<frontier_entry, pending> frontier_;
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
    , frontier_()
    , parents_{{initial, nullptr}}
    , detected_()
    , best_(initial)
    , best_cost_(get_cost.cost(initial))
    , enqueued_(0)
    , rewrites_(0) {
    enqueue_rewrites(initial, best_cost_);
}

BEST_FIRST_SEARCH_TEMPLATE bool BEST_FIRST_SEARCH::next() {
    if (frontier_.empty())
        return false;
    const pending taken = std::move(frontier_.begin()->second);
    frontier_.erase(frontier_.begin());
    ++rewrites_;
    const expr* result = taken.apply();
    DEBUG_ASSERT(result != nullptr);
    if (parents_.contains(result))
        return true;
    parents_.emplace(result, taken.source);
    const size_t cost = get_cost_.cost(result);
    if (cost < best_cost_) {
        best_ = result;
        best_cost_ = cost;
    }
    enqueue_rewrites(result, cost);
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

BEST_FIRST_SEARCH_TEMPLATE void BEST_FIRST_SEARCH::enqueue_rewrites(const expr* e, size_t cost) {
    detected_.clear();
    detect_.detect(e, detected_);
    for (rewrite& detected : detected_) {
        frontier_.emplace(frontier_entry{.cost = cost, .order = enqueued_++},
                          pending{.apply = std::move(detected), .source = e});
        if (frontier_.size() > frontier_capacity_)
            frontier_.erase(std::prev(frontier_.end()));
    }
}

#undef BEST_FIRST_SEARCH
#undef BEST_FIRST_SEARCH_TEMPLATE

#endif
