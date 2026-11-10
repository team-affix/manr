#ifndef POLY_POINT_EVALUATOR_HPP
#define POLY_POINT_EVALUATOR_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_symbols.hpp"

// Evaluates a polynomial expression at an input `point(v1, ..., vk)`, binding the i-th variable to the i-th
// coordinate. Goes through the normal form (memoized by the expander), so any shape the rules produce evaluates the
// same. No value for anything outside polynomial algebra (extension terms, piecewises) or using other variables.
//
// A coordinate holding a hole (an input `point(?1)` of data whose key is unknown) gives the residual: the polynomial
// with the literal coordinates evaluated and the others substituted, `?1^2 + 1` for `#0^2 + 1` at `point(?1)`, which a
// matcher solves against the label. Any other coordinate that is not a literal gives no value.
template<typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct poly_point_evaluator {
    poly_point_evaluator(IFindPolynomial& find_polynomial,
                         IFindLiteralValue& find_literal_value,
                         IMakeLiteral& make_literal,
                         IMakeFunctor& make_functor,
                         poly_symbols symbols,
                         std::vector<uint32_t> variables);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
private:
    const expr* make_residual(const poly_polynomial& polynomial, const std::map<uint32_t, integer>& coordinates,
                              const std::map<uint32_t, const expr*>& unknowns);
    bool holds_hole(const expr* e);

    IFindPolynomial& find_polynomial_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    poly_symbols symbols_;
    std::vector<uint32_t> variables_;
};

#define POLY_POINT_EVALUATOR_TEMPLATE \
    template<typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define POLY_POINT_EVALUATOR poly_point_evaluator<IFindPolynomial, IFindLiteralValue, IMakeLiteral, IMakeFunctor>

POLY_POINT_EVALUATOR_TEMPLATE POLY_POINT_EVALUATOR::poly_point_evaluator(
    IFindPolynomial& find_polynomial,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    poly_symbols symbols,
    std::vector<uint32_t> variables)
    : find_polynomial_(find_polynomial)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variables_(std::move(variables)) {}

POLY_POINT_EVALUATOR_TEMPLATE std::optional<const expr*> POLY_POINT_EVALUATOR::evaluate_at(
    const expr* e, const expr* input) {
    const expr::functor* point = std::get_if<expr::functor>(&input->content);
    if (point == nullptr || point->args.size() != variables_.size())
        return std::nullopt;
    const std::optional<poly_polynomial> polynomial = find_polynomial_.find_polynomial(e);
    if (!polynomial.has_value())
        return std::nullopt;
    std::map<uint32_t, integer> coordinates;
    std::map<uint32_t, const expr*> unknowns;
    for (size_t index = 0; index < variables_.size(); ++index) {
        const std::optional<integer> coordinate = find_literal_value_.find_value(point->args[index]);
        if (coordinate.has_value()) {
            coordinates.emplace(variables_[index], coordinate.value());
            continue;
        }
        if (!holds_hole(point->args[index]))
            return std::nullopt;
        unknowns.emplace(variables_[index], point->args[index]);
    }
    if (!unknowns.empty())
        return make_residual(polynomial.value(), coordinates, unknowns);
    integer sum = 0;
    for (const auto& [monomial, coefficient] : polynomial.value().terms) {
        integer term = coefficient;
        for (const poly_power& power : monomial.powers) {
            const auto coordinate = coordinates.find(power.variable);
            if (coordinate == coordinates.end())
                return std::nullopt;
            term *= boost::multiprecision::pow(coordinate->second, power.exponent);
        }
        sum += term;
    }
    return make_literal_.make_literal(sum);
}

// sum of coefficient * product of unknown^exponent, the literal coordinates folded into the coefficients
POLY_POINT_EVALUATOR_TEMPLATE const expr* POLY_POINT_EVALUATOR::make_residual(
    const poly_polynomial& polynomial, const std::map<uint32_t, integer>& coordinates,
    const std::map<uint32_t, const expr*>& unknowns) {
    std::map<std::vector<std::pair<uint32_t, uint32_t>>, integer> folded;
    for (const auto& [monomial, coefficient] : polynomial.terms) {
        integer term = coefficient;
        std::vector<std::pair<uint32_t, uint32_t>> powers;
        for (const poly_power& power : monomial.powers) {
            const auto coordinate = coordinates.find(power.variable);
            if (coordinate == coordinates.end()) {
                powers.emplace_back(power.variable, power.exponent);
                continue;
            }
            term *= boost::multiprecision::pow(coordinate->second, power.exponent);
        }
        folded[powers] += term;
    }
    const expr* sum = nullptr;
    for (const auto& [powers, coefficient] : folded) {
        if (coefficient == 0)
            continue;
        const expr* term = make_literal_.make_literal(coefficient);
        for (const auto& [variable, exponent] : powers) {
            const expr* base = unknowns.at(variable);
            const expr* raised = exponent == 1 ? base
                                               : make_functor_.make_functor(symbols_.pow,
                                                     {base, make_literal_.make_literal(integer(exponent))});
            term = make_functor_.make_functor(symbols_.mul, {term, raised});
        }
        sum = sum == nullptr ? term : make_functor_.make_functor(symbols_.add, {sum, term});
    }
    return sum == nullptr ? make_literal_.make_literal(integer(0)) : sum;
}

POLY_POINT_EVALUATOR_TEMPLATE bool POLY_POINT_EVALUATOR::holds_hole(const expr* e) {
    if (std::holds_alternative<expr::var>(e->content))
        return true;
    for (const expr* arg : std::get<expr::functor>(e->content).args)
        if (holds_hole(arg))
            return true;
    return false;
}

#undef POLY_POINT_EVALUATOR
#undef POLY_POINT_EVALUATOR_TEMPLATE

#endif
