#ifndef PROPOSER_UNION_HPP
#define PROPOSER_UNION_HPP

#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/rewrite.hpp"

// Fans `detect` out to every injected rule in order. Every rule shares the one capability, so they are indexed rather
// than named (the `IEater0, IEater1` rule); a variadic pack is that rule for any arity. This is how a manifest
// assembles a rule set from rules that live in different modules. An empty union proposes nothing (a rule set with
// no root rules).
template<typename... IDetectors>
struct proposer_union {
    proposer_union(IDetectors&... detectors);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    template<size_t... Indices>
    void detect_each(std::index_sequence<Indices...>, const expr* e, std::vector<rewrite>& out);

    std::tuple<IDetectors&...> detectors_;
};

#define PROPOSER_UNION_TEMPLATE template<typename... IDetectors>
#define PROPOSER_UNION          proposer_union<IDetectors...>

PROPOSER_UNION_TEMPLATE PROPOSER_UNION::proposer_union(IDetectors&... detectors)
    : detectors_(detectors...) {}

PROPOSER_UNION_TEMPLATE void PROPOSER_UNION::detect(const expr* e, std::vector<rewrite>& out) {
    detect_each(std::index_sequence_for<IDetectors...>{}, e, out);
}

PROPOSER_UNION_TEMPLATE
template<size_t... Indices>
void PROPOSER_UNION::detect_each(std::index_sequence<Indices...>,
                                 [[maybe_unused]] const expr* e,
                                 [[maybe_unused]] std::vector<rewrite>& out) {
    (std::get<Indices>(detectors_).detect(e, out), ...);
}

#undef PROPOSER_UNION
#undef PROPOSER_UNION_TEMPLATE

#endif
