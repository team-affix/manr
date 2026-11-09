#ifndef PW_HOLE_MATCHER_HPP
#define PW_HOLE_MATCHER_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include "value_objects/expr.hpp"

// Matches a branch's output, which may hold extension terms (holes, `?k`), against the value a model produced: true
// when some binding of the holes makes them equal, consistent with `bindings`, which it extends on success only. A
// hole matches anything, or the value it is already bound to. Two functors of the same id and arity match argument by
// argument. Anything else holding exactly one hole is handed to the domain, which solves for it (`2 * ?2` against 10
// gives `?2 = 5`). An output without holes matches only the identical value.
template<typename ISolveHole>
struct pw_hole_matcher {
    pw_hole_matcher(ISolveHole& solve_hole);
    bool match(const expr* pattern, const expr* value, std::map<uint32_t, const expr*>& bindings);
private:
    bool match_within(const expr* pattern, const expr* value, std::map<uint32_t, const expr*>& bindings);
    bool bind(uint32_t hole, const expr* value, std::map<uint32_t, const expr*>& bindings);
    void collect_holes(const expr* e, std::set<uint32_t>& holes);

    ISolveHole& solve_hole_;
};

#define PW_HOLE_MATCHER_TEMPLATE template<typename ISolveHole>
#define PW_HOLE_MATCHER          pw_hole_matcher<ISolveHole>

PW_HOLE_MATCHER_TEMPLATE PW_HOLE_MATCHER::pw_hole_matcher(ISolveHole& solve_hole)
    : solve_hole_(solve_hole) {}

PW_HOLE_MATCHER_TEMPLATE bool PW_HOLE_MATCHER::match(
    const expr* pattern, const expr* value, std::map<uint32_t, const expr*>& bindings) {
    if (pattern == value)
        return true;
    std::map<uint32_t, const expr*> trial = bindings;
    if (!match_within(pattern, value, trial))
        return false;
    bindings = trial;
    return true;
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
    if (holes.empty())
        return false;
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
    if (holes.size() != 1)
        return false;
    const uint32_t only = *holes.begin();
    const std::optional<const expr*> solved = solve_hole_.solve_hole(pattern, only, value);
    if (!solved.has_value())
        return false;
    return bind(only, solved.value(), bindings);
}

PW_HOLE_MATCHER_TEMPLATE bool PW_HOLE_MATCHER::bind(
    uint32_t hole, const expr* value, std::map<uint32_t, const expr*>& bindings) {
    const auto bound = bindings.find(hole);
    if (bound != bindings.end())
        return bound->second == value;
    bindings.emplace(hole, value);
    return true;
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
