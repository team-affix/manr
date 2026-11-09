#ifndef DETECTION_MEMO_HPP
#define DETECTION_MEMO_HPP

#include <cstddef>
#include <unordered_map>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/rewrite.hpp"

// Detection, remembered per expression. Expressions are interned, so an expression's rewrites depend only on its
// address: the first `detect` of an expression runs the injected detection and keeps its rewrites; every later one
// hands out the same rewrites without detecting again. Each rewrite handed out also remembers its result, so a rewrite
// is performed at most once. For searches that meet the same expression many times, as MCTS does at every step of
// every simulation from the same root.
//
// Memory grows with the distinct expressions detected; nothing is evicted. The map is only looked up, never iterated,
// so the order of the rewrites is the detection's and the search stays deterministic.
template<typename IDetect>
struct detection_memo {
    detection_memo(IDetect& detect);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    struct entry {
        std::vector<rewrite> rewrites;
        std::vector<const expr*> results;
    };
    using map_t = std::unordered_map<const expr*, entry>;

    IDetect& detect_;
    map_t entries_;
};

#define DETECTION_MEMO_TEMPLATE template<typename IDetect>
#define DETECTION_MEMO detection_memo<IDetect>

DETECTION_MEMO_TEMPLATE DETECTION_MEMO::detection_memo(IDetect& detect)
    : detect_(detect)
    , entries_() {}

DETECTION_MEMO_TEMPLATE void DETECTION_MEMO::detect(const expr* e, std::vector<rewrite>& out) {
    const auto [position, inserted] = entries_.try_emplace(e);
    entry& found = position->second;
    if (inserted) {
        detect_.detect(e, found.rewrites);
        found.results.assign(found.rewrites.size(), nullptr);
    }
    // map entries keep their address, so a rewrite can point at its entry
    entry* stable = &found;
    for (size_t index = 0; index < found.rewrites.size(); ++index)
        out.push_back([stable, index]() {
            if (stable->results[index] == nullptr)
                stable->results[index] = stable->rewrites[index]();
            return stable->results[index];
        });
}

#undef DETECTION_MEMO
#undef DETECTION_MEMO_TEMPLATE

#endif
