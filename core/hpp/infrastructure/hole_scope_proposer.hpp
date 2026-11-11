#ifndef HOLE_SCOPE_PROPOSER_HPP
#define HOLE_SCOPE_PROPOSER_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/rewrite.hpp"

// Lifts a root rule set (rules that bind holes, so they must see every occurrence of a hole) to every subterm that
// holds every occurrence of its own holes: binding a hole there changes nothing outside it, so the subterm may be
// treated as a whole expression. A subterm without holes, and a bare hole, are skipped. The expression itself is such
// a subterm, so this also serves as the root list. Like `subterm_proposer`, each rewrite rebuilds the path from its
// subterm up only when performed. Hole counts are memoized per expression (expressions are interned).
template<typename IDetectRoot, typename IMakeFunctor>
struct hole_scope_proposer {
    hole_scope_proposer(IDetectRoot& detect_root, IMakeFunctor& make_functor);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    using counts_t = std::map<uint32_t, size_t>;

    void detect_within(const expr* e, const counts_t& whole, std::vector<rewrite>& out);
    const counts_t& find_counts(const expr* e);

    IDetectRoot& detect_root_;
    IMakeFunctor& make_functor_;
    std::unordered_map<const expr*, counts_t> counts_;
};

#define HOLE_SCOPE_PROPOSER_TEMPLATE template<typename IDetectRoot, typename IMakeFunctor>
#define HOLE_SCOPE_PROPOSER          hole_scope_proposer<IDetectRoot, IMakeFunctor>

HOLE_SCOPE_PROPOSER_TEMPLATE HOLE_SCOPE_PROPOSER::hole_scope_proposer(IDetectRoot& detect_root,
                                                                      IMakeFunctor& make_functor)
    : detect_root_(detect_root)
    , make_functor_(make_functor)
    , counts_() {}

HOLE_SCOPE_PROPOSER_TEMPLATE void HOLE_SCOPE_PROPOSER::detect(const expr* e, std::vector<rewrite>& out) {
    const counts_t& whole = find_counts(e);
    detect_within(e, whole, out);
}

HOLE_SCOPE_PROPOSER_TEMPLATE void HOLE_SCOPE_PROPOSER::detect_within(const expr* e,
                                                                     const counts_t& whole,
                                                                     std::vector<rewrite>& out) {
    const counts_t& counts = find_counts(e);
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (counts.empty() || f == nullptr)
        return;
    bool owned = true;
    for (const auto& [hole, count] : counts)
        owned = owned && whole.at(hole) == count;
    if (owned)
        detect_root_.detect(e, out);
    for (size_t i = 0; i < f->args.size(); ++i) {
        std::vector<rewrite> arg_rewrites;
        detect_within(f->args[i], whole, arg_rewrites);
        for (rewrite& arg_rewrite : arg_rewrites) {
            out.push_back([this, f, i, inner = std::move(arg_rewrite)]() -> const expr* {
                std::vector<const expr*> args = f->args;
                args[i] = inner();
                return make_functor_.make_functor(f->id, args);
            });
        }
    }
}

// How often each hole occurs in e. References stay valid: the map's nodes never move.
HOLE_SCOPE_PROPOSER_TEMPLATE const typename HOLE_SCOPE_PROPOSER::counts_t& HOLE_SCOPE_PROPOSER::find_counts(
    const expr* e) {
    const auto known = counts_.find(e);
    if (known != counts_.end())
        return known->second;
    counts_t counts;
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr)
        counts.emplace(hole->index, 1);
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const size_t arity = f == nullptr ? 0 : f->args.size();
    for (size_t i = 0; i < arity; ++i)
        for (const auto& [index, count] : find_counts(f->args[i]))
            counts[index] += count;
    return counts_.emplace(e, std::move(counts)).first->second;
}

#undef HOLE_SCOPE_PROPOSER
#undef HOLE_SCOPE_PROPOSER_TEMPLATE

#endif
