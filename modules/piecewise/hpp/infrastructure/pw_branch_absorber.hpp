#ifndef PW_BRANCH_ABSORBER_HPP
#define PW_BRANCH_ABSORBER_HPP

#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_symbols.hpp"

// Local rule: drops, in one step, every branch of a piecewise that its otherwise already reproduces.
template<typename IFindUncovered, typename IAssemble>
struct pw_branch_absorber {
    pw_branch_absorber(IFindUncovered& find_uncovered, IAssemble& assemble, pw_symbols symbols);
    void propose(const expr* e, std::vector<const expr*>& out);
private:
    IFindUncovered& find_uncovered_;
    IAssemble& assemble_;
    pw_symbols symbols_;
};

#define PW_BRANCH_ABSORBER_TEMPLATE template<typename IFindUncovered, typename IAssemble>
#define PW_BRANCH_ABSORBER          pw_branch_absorber<IFindUncovered, IAssemble>

PW_BRANCH_ABSORBER_TEMPLATE PW_BRANCH_ABSORBER::pw_branch_absorber(
    IFindUncovered& find_uncovered, IAssemble& assemble, pw_symbols symbols)
    : find_uncovered_(find_uncovered)
    , assemble_(assemble)
    , symbols_(symbols) {}

PW_BRANCH_ABSORBER_TEMPLATE void PW_BRANCH_ABSORBER::propose(const expr* e, std::vector<const expr*>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || f->id != symbols_.piecewise)
        return;
    const std::vector<const expr*> branches(f->args.begin(), f->args.end() - 1);
    const expr* otherwise = f->args.back();
    const std::vector<const expr*> uncovered = find_uncovered_.find_uncovered(branches, otherwise);
    if (uncovered.size() == branches.size())
        return;
    out.push_back(assemble_.assemble(uncovered, otherwise));
}

#undef PW_BRANCH_ABSORBER
#undef PW_BRANCH_ABSORBER_TEMPLATE

#endif
