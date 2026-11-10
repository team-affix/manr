#ifndef POLY_COMPONENT_SPLITTER_HPP
#define POLY_COMPONENT_SPLITTER_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Detector: splits a sum into parts that share no variables (connected components of "appear in one term
// together"), proposing the sum regrouped as `part + part + ... + constant` with each part a subterm of its own.
// It doesn't shrink anything by itself. It gives the other detectors subterms they can work on, where the whole sum
// matches none of their patterns: `x^2 + 2x + y^2 + 2y + 2` -> `(x^2 + 2x) + (y^2 + 2y) + 2`.
template<typename IFindPolynomial, typename IRender, typename IMakeFunctor, typename IMakeLiteral>
struct poly_component_splitter {
    poly_component_splitter(IFindPolynomial& find_polynomial,
                            IRender& render,
                            IMakeFunctor& make_functor,
                            IMakeLiteral& make_literal,
                            poly_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* regroup(const expr* e, const std::map<uint32_t, poly_polynomial>& parts, const integer& constant);
    static uint32_t find_representative(std::map<uint32_t, uint32_t>& parents, uint32_t variable);

    IFindPolynomial& find_polynomial_;
    IRender& render_;
    IMakeFunctor& make_functor_;
    IMakeLiteral& make_literal_;
    poly_symbols symbols_;
};

#define POLY_COMPONENT_SPLITTER_TEMPLATE \
    template<typename IFindPolynomial, typename IRender, typename IMakeFunctor, typename IMakeLiteral>
#define POLY_COMPONENT_SPLITTER poly_component_splitter<IFindPolynomial, IRender, IMakeFunctor, IMakeLiteral>

POLY_COMPONENT_SPLITTER_TEMPLATE POLY_COMPONENT_SPLITTER::poly_component_splitter(
    IFindPolynomial& find_polynomial, IRender& render, IMakeFunctor& make_functor, IMakeLiteral& make_literal,
    poly_symbols symbols)
    : find_polynomial_(find_polynomial)
    , render_(render)
    , make_functor_(make_functor)
    , make_literal_(make_literal)
    , symbols_(symbols) {}

POLY_COMPONENT_SPLITTER_TEMPLATE void POLY_COMPONENT_SPLITTER::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || (f->id != symbols_.add && f->id != symbols_.sub))
        return;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (!polynomial.has_value())
        return;
    std::map<uint32_t, uint32_t> parents;
    for (const auto& [monomial, coefficient] : polynomial.value().terms) {
        for (const poly_power& power : monomial.powers) {
            const uint32_t joined = find_representative(parents, power.variable);
            parents[joined] = find_representative(parents, monomial.powers.front().variable);
        }
    }
    std::map<uint32_t, poly_polynomial> parts;
    integer constant = 0;
    for (const auto& [monomial, coefficient] : polynomial.value().terms) {
        if (monomial.powers.empty()) { constant = coefficient; continue; }
        parts[find_representative(parents, monomial.powers.front().variable)].terms.emplace(monomial, coefficient);
    }
    if (parts.size() < 2)
        return;
    const expr* regrouped = regroup(e, parts, constant);
    if (regrouped == nullptr)
        return;
    out.push_back(ready(regrouped));
}

POLY_COMPONENT_SPLITTER_TEMPLATE const expr* POLY_COMPONENT_SPLITTER::regroup(
    const expr* e, const std::map<uint32_t, poly_polynomial>& parts, const integer& constant) {
    const expr* sum = nullptr;
    for (const auto& [representative, part] : parts) {
        const bool negative = sum != nullptr && part.terms.rbegin()->second < 0;
        poly_polynomial magnitude = part;
        for (auto& [monomial, coefficient] : magnitude.terms)
            coefficient = negative ? integer(-coefficient) : coefficient;
        const expr* rendered = render_.render(magnitude);
        sum = sum == nullptr ? rendered
                             : make_functor_.make_functor(negative ? symbols_.sub : symbols_.add, {sum, rendered});
    }
    if (constant != 0)
        sum = make_functor_.make_functor(
            constant < 0 ? symbols_.sub : symbols_.add, {sum, make_literal_.make_literal(constant < 0 ? integer(-constant) : constant)});
    if (sum == e)
        return nullptr;
    return sum;
}

POLY_COMPONENT_SPLITTER_TEMPLATE uint32_t POLY_COMPONENT_SPLITTER::find_representative(
    std::map<uint32_t, uint32_t>& parents, uint32_t variable) {
    if (!parents.contains(variable))
        parents[variable] = variable;
    while (parents[variable] != variable)
        variable = parents[variable];
    return variable;
}

#undef POLY_COMPONENT_SPLITTER
#undef POLY_COMPONENT_SPLITTER_TEMPLATE

#endif
