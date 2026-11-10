#ifndef PW_HOLE_MATCHER_HPP
#define PW_HOLE_MATCHER_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"

// Matches a branch's output, which may hold extension terms (holes, `?k`), against the value a model produced: true
// when some binding of the holes makes them equal, consistent with `bindings`, which it extends on success only. The
// holes `bindings` already gives are substituted first, on both sides, so a label settled by earlier bindings is
// compared by the domain (`7 - ?2` with `?2 = 2` against 5). A hole matches anything. Two functors of the same id and
// arity match argument by argument. Anything else holding holes is handed to the domain, which solves for them
// (`2 * ?2` against 10 gives `?2 = 5`), possibly in terms of other holes (`?1 + ?2` against 7 gives `?2 = 7 - ?1`),
// which a later match pins. Bindings stay closed: no bound value holds a bound hole. An output without holes matches
// only the identical value. `substitute_holes` applies a binding to any expression.
template<typename ISolveHoles, typename IMakeFunctor>
struct pw_hole_matcher {
    pw_hole_matcher(ISolveHoles& solve_holes, IMakeFunctor& make_functor);
    bool match(const expr* pattern, const expr* value, std::map<uint32_t, const expr*>& bindings);
    const expr* substitute_holes(const expr* e, const std::map<uint32_t, const expr*>& bindings);
private:
    bool match_within(const expr* pattern, const expr* value, std::map<uint32_t, const expr*>& bindings);
    bool bind(uint32_t hole, const expr* value, std::map<uint32_t, const expr*>& bindings);
    void close(std::map<uint32_t, const expr*>& bindings);
    void collect_holes(const expr* e, std::set<uint32_t>& holes);

    ISolveHoles& solve_holes_;
    IMakeFunctor& make_functor_;
};

#define PW_HOLE_MATCHER_TEMPLATE template<typename ISolveHoles, typename IMakeFunctor>
#define PW_HOLE_MATCHER          pw_hole_matcher<ISolveHoles, IMakeFunctor>

PW_HOLE_MATCHER_TEMPLATE PW_HOLE_MATCHER::pw_hole_matcher(ISolveHoles& solve_holes, IMakeFunctor& make_functor)
    : solve_holes_(solve_holes)
    , make_functor_(make_functor) {}

PW_HOLE_MATCHER_TEMPLATE bool PW_HOLE_MATCHER::match(
    const expr* pattern, const expr* value, std::map<uint32_t, const expr*>& bindings) {
    if (pattern == value)
        return true;
    const expr* settled_pattern = substitute_holes(pattern, bindings);
    const expr* settled_value = substitute_holes(value, bindings);
    if (settled_pattern == settled_value)
        return true;
    std::set<uint32_t> holes;
    collect_holes(settled_pattern, holes);
    collect_holes(settled_value, holes);
    if (holes.empty()) {
        const bool settled = settled_pattern != pattern || settled_value != value;
        return settled && solve_holes_.solve_holes(settled_pattern, settled_value, bindings).has_value();
    }
    std::map<uint32_t, const expr*> trial = bindings;
    if (!match_within(settled_pattern, settled_value, trial))
        return false;
    close(trial);
    bindings = trial;
    return true;
}

PW_HOLE_MATCHER_TEMPLATE const expr* PW_HOLE_MATCHER::substitute_holes(
    const expr* e, const std::map<uint32_t, const expr*>& bindings) {
    if (bindings.empty())
        return e;
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        const auto bound = bindings.find(hole->index);
        return bound == bindings.end() ? e : bound->second;
    }
    const expr::functor& f = std::get<expr::functor>(e->content);
    std::vector<const expr*> args;
    args.reserve(f.args.size());
    bool changed = false;
    for (const expr* arg : f.args) {
        args.push_back(substitute_holes(arg, bindings));
        changed = changed || args.back() != arg;
    }
    return changed ? make_functor_.make_functor(f.id, args) : e;
}

PW_HOLE_MATCHER_TEMPLATE bool PW_HOLE_MATCHER::match_within(
    const expr* pattern, const expr* value, std::map<uint32_t, const expr*>& bindings) {
    if (pattern == value)
        return true;
    const expr::var* hole = std::get_if<expr::var>(&pattern->content);
    if (hole != nullptr)
        return bind(hole->index, value, bindings);
    std::set<uint32_t> holes;
    collect_holes(pattern, holes);
    if (holes.empty()) {
        std::set<uint32_t> value_holes;
        collect_holes(value, value_holes);
        if (value_holes.empty())
            return false;
        return match_within(value, pattern, bindings);
    }
    const expr::functor& p = std::get<expr::functor>(pattern->content);
    const expr::functor* v = std::get_if<expr::functor>(&value->content);
    const bool alike = v != nullptr && v->id == p.id && v->args.size() == p.args.size();
    if (alike) {
        std::map<uint32_t, const expr*> trial = bindings;
        bool matched = true;
        for (size_t position = 0; matched && position < p.args.size(); ++position)
            matched = match_within(p.args[position], v->args[position], trial);
        if (matched) {
            bindings = trial;
            return true;
        }
    }
    const std::optional<std::map<uint32_t, const expr*>> solved = solve_holes_.solve_holes(pattern, value, bindings);
    if (!solved.has_value())
        return false;
    for (const auto& [hole, bound] : solved.value())
        if (!bind(hole, bound, bindings))
            return false;
    return true;
}

PW_HOLE_MATCHER_TEMPLATE bool PW_HOLE_MATCHER::bind(
    uint32_t hole, const expr* value, std::map<uint32_t, const expr*>& bindings) {
    const auto bound = bindings.find(hole);
    if (bound != bindings.end())
        return bound->second == value;
    bindings.emplace(hole, value);
    return true;
}

// substitutes the bindings into one another until none holds a bound hole (each pass settles one more level)
PW_HOLE_MATCHER_TEMPLATE void PW_HOLE_MATCHER::close(std::map<uint32_t, const expr*>& bindings) {
    for (size_t pass = 0; pass < bindings.size(); ++pass) {
        bool changed = false;
        for (auto& [hole, bound] : bindings) {
            const expr* settled = substitute_holes(bound, bindings);
            changed = changed || settled != bound;
            bound = settled;
        }
        if (!changed)
            return;
    }
}

PW_HOLE_MATCHER_TEMPLATE void PW_HOLE_MATCHER::collect_holes(const expr* e, std::set<uint32_t>& holes) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        holes.insert(hole->index);
        return;
    }
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        collect_holes(arg, holes);
}

#undef PW_HOLE_MATCHER
#undef PW_HOLE_MATCHER_TEMPLATE

#endif
