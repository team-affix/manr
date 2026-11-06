#ifndef PROPOSER_UNION_HPP
#define PROPOSER_UNION_HPP

#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"

// Fans `propose` out to every injected proposer in order. Every proposer shares the one capability, so they are
// indexed rather than named (the `IEater0, IEater1` rule); a variadic pack is that rule for any arity.
// This is how a manifest assembles a rule set from rules that live in different modules.
template<typename... IProposers>
struct proposer_union {
    proposer_union(IProposers&... proposers);
    void propose(const expr* e, std::vector<const expr*>& out);
private:
    template<size_t... Indices>
    void propose_each(std::index_sequence<Indices...>, const expr* e, std::vector<const expr*>& out);

    std::tuple<IProposers&...> proposers_;
};

#define PROPOSER_UNION_TEMPLATE template<typename... IProposers>
#define PROPOSER_UNION          proposer_union<IProposers...>

PROPOSER_UNION_TEMPLATE PROPOSER_UNION::proposer_union(IProposers&... proposers)
    : proposers_(proposers...) {}

PROPOSER_UNION_TEMPLATE void PROPOSER_UNION::propose(const expr* e, std::vector<const expr*>& out) {
    propose_each(std::index_sequence_for<IProposers...>{}, e, out);
}

PROPOSER_UNION_TEMPLATE
template<size_t... Indices>
void PROPOSER_UNION::propose_each(std::index_sequence<Indices...>, const expr* e, std::vector<const expr*>& out) {
    (std::get<Indices>(proposers_).propose(e, out), ...);
}

#undef PROPOSER_UNION
#undef PROPOSER_UNION_TEMPLATE

#endif
