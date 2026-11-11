#ifndef ABS_HELPER_ABSTRACTOR_HPP
#define ABS_HELPER_ABSTRACTOR_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <variant>
#include <vector>
#include "value_objects/abs_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/rewrite.hpp"

constexpr size_t k_abs_default_max_abstracted_size = 200;
constexpr uint32_t k_abs_cut_marker = 1u << 30;

// Local rule: names a fragment that recurs with different arguments as a helper, `plans.md` (abstraction): a fragment
// is a subterm with one position cut out (its input), and when two or more subterms are that fragment around
// different arguments, e becomes `let(lam(F), e')`, F the fragment with its input as `#0` and each occurrence in e'
// the call `app(#d, a)` (d the binders between the `let` and the occurrence, a the occurrence's argument). Everything
// in e moves under one more binder, so its free indices move out by one (`shift`). The fragment must be closed (no
// index of its own but its input, which is not under its binders), so it means the same at every occurrence. Fires
// only when smaller: k occurrences of a fragment of B nodes (its cut counted as one) save k * (B - 3) - B - 2. The
// sum of squares of two lists is one helper applied twice (#75). Nothing for an e of more than `max_size` nodes.
template<typename IShiftIndices, typename IFindIndex, typename IMakeIndex, typename ICountBinders, typename IMakeFunctor,
         typename IMakeVar, typename IFindCost>
struct abs_helper_abstractor {
    abs_helper_abstractor(IShiftIndices& shift_indices,
                          IFindIndex& find_index,
                          IMakeIndex& make_index,
                          ICountBinders& count_binders,
                          IMakeFunctor& make_functor,
                          IMakeVar& make_var,
                          IFindCost& find_cost,
                          abs_symbols symbols,
                          size_t max_size = k_abs_default_max_abstracted_size);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    // where a fragment occurs: the subterm, its argument, and the path from e to it
    struct occurrence {
        const expr* subterm;
        const expr* argument;
        std::vector<size_t> path;
        std::vector<size_t> cut;
    };
    void collect(const expr* e, std::vector<size_t>& path, std::map<const expr*, std::vector<occurrence>>& fragments);
    void cut(const expr* e, const expr* subterm, std::vector<size_t>& at,
             std::map<const expr*, std::vector<occurrence>>& fragments, const std::vector<size_t>& path);
    const expr* make_fragment(const expr* e, const std::vector<size_t>& at, size_t depth, const expr* replacement);
    const expr* replace_at(const expr* e, const std::vector<size_t>& path, size_t at, size_t depth,
                           const expr* argument);
    bool is_closed(const expr* e, size_t binders) const;
    bool holds_function(const expr* e) const;
    size_t count_binders_at(const expr* parent, size_t position) const;

    IShiftIndices& shift_indices_;
    IFindIndex& find_index_;
    IMakeIndex& make_index_;
    ICountBinders& count_binders_;
    IMakeFunctor& make_functor_;
    IMakeVar& make_var_;
    IFindCost& find_cost_;
    abs_symbols symbols_;
    size_t max_size_;
    std::unordered_map<const expr*, const expr*> abstracted_;
};

#define ABS_HELPER_ABSTRACTOR_TEMPLATE                                                                         \
    template<typename IShiftIndices, typename IFindIndex, typename IMakeIndex, typename ICountBinders,         \
             typename IMakeFunctor, typename IMakeVar, typename IFindCost>
#define ABS_HELPER_ABSTRACTOR \
    abs_helper_abstractor<IShiftIndices, IFindIndex, IMakeIndex, ICountBinders, IMakeFunctor, IMakeVar, IFindCost>

ABS_HELPER_ABSTRACTOR_TEMPLATE ABS_HELPER_ABSTRACTOR::abs_helper_abstractor(IShiftIndices& shift_indices,
                                                                            IFindIndex& find_index,
                                                                            IMakeIndex& make_index,
                                                                            ICountBinders& count_binders,
                                                                            IMakeFunctor& make_functor,
                                                                            IMakeVar& make_var,
                                                                            IFindCost& find_cost,
                                                                            abs_symbols symbols,
                                                                            size_t max_size)
    : shift_indices_(shift_indices)
    , find_index_(find_index)
    , make_index_(make_index)
    , count_binders_(count_binders)
    , make_functor_(make_functor)
    , make_var_(make_var)
    , find_cost_(find_cost)
    , symbols_(symbols)
    , max_size_(max_size)
    , abstracted_() {}

ABS_HELPER_ABSTRACTOR_TEMPLATE void ABS_HELPER_ABSTRACTOR::detect(const expr* e, std::vector<rewrite>& out) {
    const size_t cost = find_cost_.cost(e);
    if (cost > max_size_ || cost < 10)
        return;
    // a helper names a piece of a program: only an expression holding a function is read, and once
    const auto known = abstracted_.find(e);
    if (known != abstracted_.end()) {
        if (known->second != nullptr)
            out.push_back(ready(known->second));
        return;
    }
    if (!holds_function(e)) {
        abstracted_.emplace(e, nullptr);
        return;
    }
    // e as it will sit under the `let`: every free index one further out
    const expr* moved = shift_indices_.shift(e, 1, 0);
    std::map<const expr*, std::vector<occurrence>> fragments;
    std::vector<size_t> path;
    collect(moved, path, fragments);
    std::optional<const expr*> best;
    size_t best_cost = cost;
    for (const auto& [fragment, occurrences] : fragments) {
        // occurrences that do not overlap (none on another's path), with at least two arguments
        std::vector<const occurrence*> kept;
        for (const occurrence& o : occurrences) {
            bool overlaps = false;
            for (const occurrence* k : kept) {
                const size_t shorter = std::min(k->path.size(), o.path.size());
                overlaps = overlaps || std::equal(k->path.begin(), k->path.begin() + static_cast<long>(shorter),
                                                  o.path.begin());
            }
            if (!overlaps)
                kept.push_back(&o);
        }
        std::set<const expr*> arguments;
        for (const occurrence* k : kept)
            arguments.insert(k->argument);
        if (kept.size() < 2 || arguments.size() < 2)
            continue;
        // the helper: the fragment with its cut as its parameter `#0` (the cut is under none of its binders)
        const expr* helper = make_functor_.make_functor(
            symbols_.lambda, {make_fragment(kept[0]->subterm, kept[0]->cut, 0, make_index_.make_index(0))});
        const expr* rewritten = moved;
        for (const occurrence* k : kept)
            rewritten = replace_at(rewritten, k->path, 0, 0, k->argument);
        const expr* abstracted = make_functor_.make_functor(symbols_.binding, {helper, rewritten});
        const size_t abstracted_cost = find_cost_.cost(abstracted);
        if (abstracted_cost >= best_cost)
            continue;
        best = abstracted;
        best_cost = abstracted_cost;
    }
    abstracted_.emplace(e, best.has_value() ? best.value() : nullptr);
    if (best.has_value())
        out.push_back(ready(best.value()));
}

ABS_HELPER_ABSTRACTOR_TEMPLATE bool ABS_HELPER_ABSTRACTOR::holds_function(const expr* e) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return false;
    if (f->id == symbols_.lambda || f->id == symbols_.lambdas)
        return true;
    for (const expr* arg : f->args)
        if (holds_function(arg))
            return true;
    return false;
}

// every fragment of every subterm: the subterm with one position cut, that position not under the subterm's binders
ABS_HELPER_ABSTRACTOR_TEMPLATE void ABS_HELPER_ABSTRACTOR::collect(
    const expr* e, std::vector<size_t>& path, std::map<const expr*, std::vector<occurrence>>& fragments) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->args.empty())
        return;
    if (find_cost_.cost(e) >= 5) {
        std::vector<size_t> at;
        cut(e, e, at, fragments, path);
    }
    for (size_t i = 0; i < f->args.size(); ++i) {
        path.push_back(i);
        collect(f->args[i], path, fragments);
        path.pop_back();
    }
}

// the fragments of `subterm` cut at each position reachable from `at` without entering a binder of the subterm
ABS_HELPER_ABSTRACTOR_TEMPLATE void ABS_HELPER_ABSTRACTOR::cut(
    const expr* e, const expr* subterm, std::vector<size_t>& at,
    std::map<const expr*, std::vector<occurrence>>& fragments, const std::vector<size_t>& path) {
    if (!at.empty()) {
        const expr* fragment = make_fragment(subterm, at, 0, make_var_.make_var(k_abs_cut_marker));
        if (is_closed(fragment, 0))
            fragments[fragment].push_back(occurrence{.subterm = subterm, .argument = e, .path = path, .cut = at});
    }
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return;
    for (size_t i = 0; i < f->args.size(); ++i) {
        if (count_binders_at(e, i) > 0)
            continue;
        at.push_back(i);
        cut(f->args[i], subterm, at, fragments, path);
        at.pop_back();
    }
}

// e with the position `at` (from `depth` on) replaced
ABS_HELPER_ABSTRACTOR_TEMPLATE const expr* ABS_HELPER_ABSTRACTOR::make_fragment(const expr* e,
                                                                                const std::vector<size_t>& at,
                                                                                size_t depth,
                                                                                const expr* replacement) {
    if (depth == at.size())
        return replacement;
    const expr::functor& f = std::get<expr::functor>(e->content);
    std::vector<const expr*> args = f.args;
    args[at[depth]] = make_fragment(f.args[at[depth]], at, depth + 1, replacement);
    return make_functor_.make_functor(f.id, args);
}

// e with the occurrence at `path` (from position `at` on) replaced by the call `app(#d, argument)`, d the binders on
// the way down: the helper the `let` binds is `#0` at the `let`'s body and one further out under each binder
ABS_HELPER_ABSTRACTOR_TEMPLATE const expr* ABS_HELPER_ABSTRACTOR::replace_at(const expr* e,
                                                                             const std::vector<size_t>& path,
                                                                             size_t at, size_t depth,
                                                                             const expr* argument) {
    if (at == path.size())
        return make_functor_.make_functor(symbols_.application, {make_index_.make_index(depth), argument});
    const expr::functor& f = std::get<expr::functor>(e->content);
    std::vector<const expr*> args = f.args;
    args[path[at]] = replace_at(f.args[path[at]], path, at + 1, depth + count_binders_at(e, path[at]), argument);
    return make_functor_.make_functor(f.id, args);
}

// whether every index of e is bound within e (past `binders` binders of e's own)
ABS_HELPER_ABSTRACTOR_TEMPLATE bool ABS_HELPER_ABSTRACTOR::is_closed(const expr* e, size_t binders) const {
    const std::optional<size_t> index = find_index_.find_index(e);
    if (index.has_value())
        return index.value() < binders;
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return true;
    for (size_t i = 0; i < f->args.size(); ++i)
        if (!is_closed(f->args[i], binders + count_binders_at(e, i)))
            return false;
    return true;
}

// the binders an argument sits under, introduced by its parent: a `lam`'s or `lams`'s body its count, a `let`'s
// second argument 1, anything else 0
ABS_HELPER_ABSTRACTOR_TEMPLATE size_t ABS_HELPER_ABSTRACTOR::count_binders_at(const expr* parent,
                                                                             size_t position) const {
    const expr::functor& f = std::get<expr::functor>(parent->content);
    if (f.id == symbols_.binding)
        return position == 1 ? 1 : 0;
    return position + 1 == f.args.size() ? count_binders_.count_binders(parent) : 0;
}

#undef ABS_HELPER_ABSTRACTOR
#undef ABS_HELPER_ABSTRACTOR_TEMPLATE

#endif
