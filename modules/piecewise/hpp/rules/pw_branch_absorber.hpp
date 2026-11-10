#ifndef PW_BRANCH_ABSORBER_HPP
#define PW_BRANCH_ABSORBER_HPP

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_coverage.hpp"
#include "value_objects/pw_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Local rule: drops, in one step, every `case` branch of a piecewise that what follows it already reproduces.
// Later cases apply only at their own key, so what follows a case at its key is the `when` branches and the
// otherwise (a `when` branch before the case that applies there makes the case dead, so dropping it is sound too).
// A case whose key a later case repeats is kept: dropping it would hand its key to the later one. Detection
// evaluates the branches to decide; the rewrite only assembles the smaller piecewise. The dropped cases are reproduced
// under one binding of their labels' holes, which the result carries into the cases it keeps.
template<typename IFindUncovered, typename IAssemble, typename ISubstituteHoles>
struct pw_branch_absorber {
    pw_branch_absorber(IFindUncovered& find_uncovered,
                       IAssemble& assemble,
                       ISubstituteHoles& substitute_holes,
                       pw_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    IFindUncovered& find_uncovered_;
    IAssemble& assemble_;
    ISubstituteHoles& substitute_holes_;
    pw_symbols symbols_;
};

#define PW_BRANCH_ABSORBER_TEMPLATE template<typename IFindUncovered, typename IAssemble, typename ISubstituteHoles>
#define PW_BRANCH_ABSORBER          pw_branch_absorber<IFindUncovered, IAssemble, ISubstituteHoles>

PW_BRANCH_ABSORBER_TEMPLATE PW_BRANCH_ABSORBER::pw_branch_absorber(
    IFindUncovered& find_uncovered, IAssemble& assemble, ISubstituteHoles& substitute_holes, pw_symbols symbols)
    : find_uncovered_(find_uncovered)
    , assemble_(assemble)
    , substitute_holes_(substitute_holes)
    , symbols_(symbols) {}

PW_BRANCH_ABSORBER_TEMPLATE void PW_BRANCH_ABSORBER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.piecewise)
        return;
    const std::vector<const expr*> branches(f->args.begin(), f->args.end() - 1);
    const expr* otherwise = f->args.back();
    std::vector<const expr*> regions;
    std::vector<const expr*> candidates;
    std::set<const expr*> later_keys;
    for (auto branch = branches.rbegin(); branch != branches.rend(); ++branch) {
        const expr::functor& b = std::get<expr::functor>((*branch)->content);
        if (b.id != symbols_.branch) {
            regions.push_back(*branch);
            continue;
        }
        const bool repeated = !later_keys.insert(b.args[0]).second;
        if (repeated)
            continue;
        candidates.push_back(*branch);
    }
    std::reverse(regions.begin(), regions.end());
    std::reverse(candidates.begin(), candidates.end());
    const pw_coverage coverage = find_uncovered_.find_uncovered(candidates, assemble_.assemble(regions, otherwise));
    if (coverage.uncovered.size() == candidates.size())
        return;
    const std::set<const expr*> uncovered_set(coverage.uncovered.begin(), coverage.uncovered.end());
    const std::set<const expr*> candidate_set(candidates.begin(), candidates.end());
    std::vector<const expr*> kept;
    for (const expr* branch : branches) {
        const bool covered = candidate_set.contains(branch) && !uncovered_set.contains(branch);
        if (covered)
            continue;
        kept.push_back(branch);
    }
    const std::map<uint32_t, const expr*> bindings = coverage.bindings;
    out.push_back([this, kept, otherwise, bindings]() {
        return substitute_holes_.substitute_holes(assemble_.assemble(kept, otherwise), bindings);
    });
}

#undef PW_BRANCH_ABSORBER
#undef PW_BRANCH_ABSORBER_TEMPLATE

#endif
