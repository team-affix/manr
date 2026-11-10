#ifndef POLY_LIKE_TERMS_HPP
#define POLY_LIKE_TERMS_HPP

#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/rewrite.hpp"

// Detector: collects like terms of a sum without expanding them. The sum is flattened through + and - into signed
// summands `c * t`. Summands whose t are equal polynomials (compared by normal form, so `x*y` and `y*x` agree, or
// by identity when there is none) are merged and their coefficients added. The result is proposed when smaller.
// It is the collector for terms too big to expand profitably:
// `3 * (a + b + c)^9 - (a + b + c)^9 * 2` -> `(a + b + c)^9`.
template<typename IFindPolynomial, typename IFindLiteralValue, typename IMakeFunctor, typename IMakeLiteral,
         typename IGetCost>
struct poly_like_terms {
    poly_like_terms(IFindPolynomial& find_polynomial,
                    IFindLiteralValue& find_literal_value,
                    IMakeFunctor& make_functor,
                    IMakeLiteral& make_literal,
                    IGetCost& get_cost,
                    poly_symbols symbols);
    void detect(const expr* e, std::vector<rewrite>& out);
private:
    const expr* collect(const expr* e);
    struct group {
        integer coefficient;
        const expr* representative;
        std::optional<poly_polynomial> key;
    };

    void flatten(const expr* e, int sign, std::vector<std::pair<int, const expr*>>& summands) const;

    IFindPolynomial& find_polynomial_;
    IFindLiteralValue& find_literal_value_;
    IMakeFunctor& make_functor_;
    IMakeLiteral& make_literal_;
    IGetCost& get_cost_;
    poly_symbols symbols_;
};

#define POLY_LIKE_TERMS_TEMPLATE                                                                      \
    template<typename IFindPolynomial, typename IFindLiteralValue, typename IMakeFunctor, typename IMakeLiteral, \
             typename IGetCost>
#define POLY_LIKE_TERMS poly_like_terms<IFindPolynomial, IFindLiteralValue, IMakeFunctor, IMakeLiteral, IGetCost>

POLY_LIKE_TERMS_TEMPLATE POLY_LIKE_TERMS::poly_like_terms(
    IFindPolynomial& find_polynomial,
    IFindLiteralValue& find_literal_value,
    IMakeFunctor& make_functor,
    IMakeLiteral& make_literal,
    IGetCost& get_cost,
    poly_symbols symbols)
    : find_polynomial_(find_polynomial)
    , find_literal_value_(find_literal_value)
    , make_functor_(make_functor)
    , make_literal_(make_literal)
    , get_cost_(get_cost)
    , symbols_(symbols) {}

POLY_LIKE_TERMS_TEMPLATE void POLY_LIKE_TERMS::detect(const expr* e, std::vector<rewrite>& out) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr || (f->id != symbols_.add && f->id != symbols_.sub))
        return;
    const expr* collected = collect(e);
    if (collected == nullptr)
        return;
    out.push_back(ready(collected));
}

// The sum with like terms merged, or nullptr if nothing merges or the result is not smaller.
POLY_LIKE_TERMS_TEMPLATE const expr* POLY_LIKE_TERMS::collect(const expr* e) {
    std::vector<std::pair<int, const expr*>> summands;
    flatten(e, 1, summands);
    std::vector<group> groups;
    integer constant = 0;
    size_t literals = 0;
    for (const auto& [sign, summand] : summands) {
        const std::optional<integer> value = find_literal_value_.find_value(summand);
        if (value.has_value()) {
            constant += integer(sign * value.value());
            ++literals;
            continue;
        }
        integer coefficient = sign;
        const expr* term = summand;
        const expr::functor* product = std::get_if<expr::functor>(&summand->content);
        if (product != nullptr && product->id == symbols_.mul) {
            const std::optional<integer> left = find_literal_value_.find_value(product->args[0]);
            const std::optional<integer> right = find_literal_value_.find_value(product->args[1]);
            if (left.has_value()) { coefficient = integer(sign * left.value()); term = product->args[1]; }
            else if (right.has_value()) { coefficient = integer(sign * right.value()); term = product->args[0]; }
        }
        const std::optional<poly_polynomial> key = find_polynomial_.find_polynomial(term);
        group* match = nullptr;
        for (group& candidate : groups) {
            const bool same = key.has_value() ? candidate.key == key : candidate.representative == term;
            if (same) { match = &candidate; break; }
        }
        if (match == nullptr) {
            groups.push_back(group{.coefficient = coefficient, .representative = term, .key = key});
            continue;
        }
        match->coefficient += coefficient;
        if (get_cost_.cost(term) < get_cost_.cost(match->representative))
            match->representative = term;
    }
    const bool merged = groups.size() + literals < summands.size() || literals > 1 || (literals == 1 && constant == 0);
    if (!merged)
        return nullptr;
    const expr* sum = nullptr;
    for (const group& g : groups) {
        if (g.coefficient == 0)
            continue;
        const bool subtract = sum != nullptr && g.coefficient < 0;
        const integer magnitude = subtract ? integer(-g.coefficient) : g.coefficient;
        const expr* term = magnitude == 1
            ? g.representative
            : make_functor_.make_functor(symbols_.mul, {make_literal_.make_literal(magnitude), g.representative});
        sum = sum == nullptr ? term : make_functor_.make_functor(subtract ? symbols_.sub : symbols_.add, {sum, term});
    }
    if (sum == nullptr)
        sum = make_literal_.make_literal(constant);
    else if (constant != 0)
        sum = make_functor_.make_functor(
            constant < 0 ? symbols_.sub : symbols_.add, {sum, make_literal_.make_literal(constant < 0 ? integer(-constant) : constant)});
    if (get_cost_.cost(sum) >= get_cost_.cost(e))
        return nullptr;
    return sum;
}

POLY_LIKE_TERMS_TEMPLATE void POLY_LIKE_TERMS::flatten(
    const expr* e, int sign, std::vector<std::pair<int, const expr*>>& summands) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const bool sum = f != nullptr && (f->id == symbols_.add || f->id == symbols_.sub);
    if (!sum) {
        summands.emplace_back(sign, e);
        return;
    }
    flatten(f->args[0], sign, summands);
    flatten(f->args[1], f->id == symbols_.sub ? -sign : sign, summands);
}

#undef POLY_LIKE_TERMS
#undef POLY_LIKE_TERMS_TEMPLATE

#endif
