#ifndef PW_BRANCH_FILTER_HPP
#define PW_BRANCH_FILTER_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_coverage.hpp"
#include "value_objects/pw_symbols.hpp"

constexpr size_t k_pw_default_max_label_fillers = 24;

// Finds the branches an otherwise does not already reproduce. A branch `case(key, value)` is covered when the
// otherwise's value at `key` matches `value`: equal, or equal under a binding of the holes either holds (an unknown
// label `?1`, or `2 * ?2`), with one binding shared by every branch of the call, which is returned with them. Labels
// are matched in passes, so that a hole is pinned before it is guessed: first those holding at most one hole, then the
// rest, then once more those that failed and hold a hole (a later binding may settle them). An otherwise the domain
// cannot evaluate (e.g. one holding an extension term) covers nothing. A branch whose key holds holes and that is still
// not covered (a filter stops at `lt(0, ?1)`) is tried last with the key's unbound holes at each of `fillers` in turn
// (the domain's 0 and 1), then at each part of its own label (at most `max_label_fillers`: an element of a sorted
// list), and is covered under the first binding where the model reproduces it. A `when` branch is
// never covered: it is not a data point. Dropping a covered branch drops its holes, so this assumes they occur nowhere
// outside the piecewise.
template<typename IEvaluateAt, typename IMatchOutput, typename ISubstituteHoles>
struct pw_branch_filter {
    pw_branch_filter(IEvaluateAt& evaluate_at,
                     IMatchOutput& match_output,
                     ISubstituteHoles& substitute_holes,
                     pw_symbols symbols,
                     std::vector<const expr*> fillers);
    pw_coverage find_uncovered(const std::vector<const expr*>& branches, const expr* otherwise);
private:
    bool cover_by_filling(const expr* otherwise, const expr* key, const expr* label,
                          std::map<uint32_t, const expr*>& bindings);
    static void collect_holes(const expr* e, std::set<uint32_t>& holes);

    IEvaluateAt& evaluate_at_;
    IMatchOutput& match_output_;
    ISubstituteHoles& substitute_holes_;
    pw_symbols symbols_;
    std::vector<const expr*> fillers_;
};

#define PW_BRANCH_FILTER_TEMPLATE template<typename IEvaluateAt, typename IMatchOutput, typename ISubstituteHoles>
#define PW_BRANCH_FILTER          pw_branch_filter<IEvaluateAt, IMatchOutput, ISubstituteHoles>

PW_BRANCH_FILTER_TEMPLATE PW_BRANCH_FILTER::pw_branch_filter(IEvaluateAt& evaluate_at,
                                                             IMatchOutput& match_output,
                                                             ISubstituteHoles& substitute_holes,
                                                             pw_symbols symbols,
                                                             std::vector<const expr*> fillers)
    : evaluate_at_(evaluate_at)
    , match_output_(match_output)
    , substitute_holes_(substitute_holes)
    , symbols_(symbols)
    , fillers_(std::move(fillers)) {}

PW_BRANCH_FILTER_TEMPLATE pw_coverage PW_BRANCH_FILTER::find_uncovered(
    const std::vector<const expr*>& branches, const expr* otherwise) {
    std::map<uint32_t, const expr*> bindings;
    std::vector<std::optional<const expr*>> produced(branches.size());
    std::vector<std::set<uint32_t>> holes(branches.size());
    std::vector<bool> covered(branches.size(), false);
    std::vector<bool> open(branches.size(), false);
    std::vector<bool> fillable(branches.size(), false);
    for (size_t index = 0; index < branches.size(); ++index) {
        const expr::functor& f = std::get<expr::functor>(branches[index]->content);
        if (f.id != symbols_.branch)
            continue;
        std::set<uint32_t> key_holes;
        collect_holes(f.args[0], key_holes);
        fillable[index] = !key_holes.empty();
        produced[index] = evaluate_at_.evaluate_at(otherwise, f.args[0]);
        if (!produced[index].has_value())
            continue;
        collect_holes(f.args[1], holes[index]);
        std::set<uint32_t> produced_holes;
        collect_holes(produced[index].value(), produced_holes);
        open[index] = true;
        const bool deferred = holes[index].size() > 1;
        if (deferred)
            continue;
        open[index] = !holes[index].empty() || !produced_holes.empty();
        covered[index] = match_output_.match(f.args[1], produced[index].value(), bindings);
    }
    for (const bool retry : {false, true}) {
        for (size_t index = 0; index < branches.size(); ++index) {
            const bool waiting = open[index] && !covered[index] && (retry || holes[index].size() > 1);
            if (!waiting)
                continue;
            const expr::functor& f = std::get<expr::functor>(branches[index]->content);
            covered[index] = match_output_.match(f.args[1], produced[index].value(), bindings);
        }
    }
    for (size_t index = 0; index < branches.size(); ++index) {
        if (covered[index] || !fillable[index])
            continue;
        const expr::functor& f = std::get<expr::functor>(branches[index]->content);
        covered[index] = cover_by_filling(otherwise, f.args[0], f.args[1], bindings);
    }
    pw_coverage coverage{.uncovered = {}, .bindings = bindings};
    for (size_t index = 0; index < branches.size(); ++index)
        if (!covered[index])
            coverage.uncovered.push_back(branches[index]);
    return coverage;
}

// tries the key's unbound holes at each filler in turn: covered at the first where the model's value there matches
// the label, under the bindings and the filled holes, which then join the bindings
PW_BRANCH_FILTER_TEMPLATE bool PW_BRANCH_FILTER::cover_by_filling(const expr* otherwise, const expr* key,
                                                                  const expr* label,
                                                                  std::map<uint32_t, const expr*>& bindings) {
    std::set<uint32_t> holes;
    collect_holes(substitute_holes_.substitute_holes(key, bindings), holes);
    // the fixed fillers, then the parts of the label without holes (a sorted list's elements, a sum's value)
    std::vector<const expr*> candidates = fillers_;
    std::vector<const expr*> parts{label};
    while (!parts.empty() && candidates.size() < fillers_.size() + k_pw_default_max_label_fillers) {
        const expr* part = parts.back();
        parts.pop_back();
        std::set<uint32_t> inside;
        collect_holes(part, inside);
        if (inside.empty() && std::find(candidates.begin(), candidates.end(), part) == candidates.end())
            candidates.push_back(part);
        if (const expr::functor* f = std::get_if<expr::functor>(&part->content))
            parts.insert(parts.end(), f->args.begin(), f->args.end());
    }
    for (const expr* filler : candidates) {
        std::map<uint32_t, const expr*> trial = bindings;
        for (const uint32_t hole : holes)
            trial.emplace(hole, filler);
        const std::optional<const expr*> value =
            evaluate_at_.evaluate_at(otherwise, substitute_holes_.substitute_holes(key, trial));
        if (!value.has_value())
            continue;
        if (!match_output_.match(substitute_holes_.substitute_holes(label, trial), value.value(), trial))
            continue;
        bindings = trial;
        return true;
    }
    return false;
}

PW_BRANCH_FILTER_TEMPLATE void PW_BRANCH_FILTER::collect_holes(const expr* e, std::set<uint32_t>& holes) {
    const expr::var* hole = std::get_if<expr::var>(&e->content);
    if (hole != nullptr) {
        holes.insert(hole->index);
        return;
    }
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        collect_holes(arg, holes);
}

#undef PW_BRANCH_FILTER
#undef PW_BRANCH_FILTER_TEMPLATE

#endif
