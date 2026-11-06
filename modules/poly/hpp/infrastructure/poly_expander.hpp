#ifndef POLY_EXPANDER_HPP
#define POLY_EXPANDER_HPP

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"

// The normal form of an expression, memoized per interned address. No normal form for an extension term, a functor
// outside the vocabulary, a non-literal exponent or one above 64, or a result with too many terms.
template<typename IFindLiteralValue,
         typename IAddPolynomials,
         typename ISubtractPolynomials,
         typename IMultiplyPolynomials,
         typename IPowerPolynomial>
struct poly_expander {
    poly_expander(IFindLiteralValue& find_literal_value,
                  IAddPolynomials& add_polynomials,
                  ISubtractPolynomials& subtract_polynomials,
                  IMultiplyPolynomials& multiply_polynomials,
                  IPowerPolynomial& power_polynomial,
                  poly_symbols symbols);
    std::optional<poly_polynomial> find_polynomial(const expr* e);
private:
    std::optional<poly_polynomial> expand(const expr* e);

    IFindLiteralValue& find_literal_value_;
    IAddPolynomials& add_polynomials_;
    ISubtractPolynomials& subtract_polynomials_;
    IMultiplyPolynomials& multiply_polynomials_;
    IPowerPolynomial& power_polynomial_;
    poly_symbols symbols_;
    std::unordered_map<const expr*, std::optional<poly_polynomial>> polynomials_;
};

#define POLY_EXPANDER_TEMPLATE                                                                  \
    template<typename IFindLiteralValue, typename IAddPolynomials, typename ISubtractPolynomials, \
             typename IMultiplyPolynomials, typename IPowerPolynomial>
#define POLY_EXPANDER \
    poly_expander<IFindLiteralValue, IAddPolynomials, ISubtractPolynomials, IMultiplyPolynomials, IPowerPolynomial>

POLY_EXPANDER_TEMPLATE POLY_EXPANDER::poly_expander(
    IFindLiteralValue& find_literal_value,
    IAddPolynomials& add_polynomials,
    ISubtractPolynomials& subtract_polynomials,
    IMultiplyPolynomials& multiply_polynomials,
    IPowerPolynomial& power_polynomial,
    poly_symbols symbols)
    : find_literal_value_(find_literal_value)
    , add_polynomials_(add_polynomials)
    , subtract_polynomials_(subtract_polynomials)
    , multiply_polynomials_(multiply_polynomials)
    , power_polynomial_(power_polynomial)
    , symbols_(symbols)
    , polynomials_() {}

POLY_EXPANDER_TEMPLATE std::optional<poly_polynomial> POLY_EXPANDER::find_polynomial(const expr* e) {
    const auto found = polynomials_.find(e);
    if (found != polynomials_.end())
        return found->second;
    const std::optional<poly_polynomial> expanded = expand(e);
    polynomials_.emplace(e, expanded);
    return expanded;
}

POLY_EXPANDER_TEMPLATE std::optional<poly_polynomial> POLY_EXPANDER::expand(const expr* e) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return std::nullopt;
    if (f->args.empty()) {
        const std::optional<integer> value = find_literal_value_.find_value(e);
        poly_polynomial leaf;
        if (!value.has_value())
            leaf.terms.emplace(poly_monomial{.powers = {poly_power{.variable = f->id, .exponent = 1}}}, 1);
        else if (value.value() != 0)
            leaf.terms.emplace(poly_monomial{}, value.value());
        return leaf;
    }
    if (f->args.size() != 2)
        return std::nullopt;
    const std::optional<poly_polynomial> lhs = find_polynomial(f->args[0]);
    if (!lhs.has_value())
        return std::nullopt;
    if (f->id == symbols_.pow) {
        const std::optional<integer> exponent = find_literal_value_.find_value(f->args[1]);
        const bool usable = exponent.has_value() && exponent.value() >= 0 && exponent.value() <= 64;
        if (!usable)
            return std::nullopt;
        return power_polynomial_.power(lhs.value(), exponent.value().convert_to<uint32_t>());
    }
    const std::optional<poly_polynomial> rhs = find_polynomial(f->args[1]);
    if (!rhs.has_value())
        return std::nullopt;
    if (f->id == symbols_.add)
        return add_polynomials_.add(lhs.value(), rhs.value());
    if (f->id == symbols_.sub)
        return subtract_polynomials_.subtract(lhs.value(), rhs.value());
    if (f->id == symbols_.mul)
        return multiply_polynomials_.multiply(lhs.value(), rhs.value());
    return std::nullopt;
}

#undef POLY_EXPANDER
#undef POLY_EXPANDER_TEMPLATE

#endif
