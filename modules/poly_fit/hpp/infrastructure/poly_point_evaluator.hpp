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

// Evaluates a polynomial expression at an input `point(v1, ..., vk)`, binding the i-th variable to the i-th
// coordinate. Goes through the normal form (memoized by the expander), so any shape the rules produce evaluates the
// same. No value for anything outside polynomial algebra (extension terms, piecewises) or using other variables.
template<typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral>
struct poly_point_evaluator {
    poly_point_evaluator(IFindPolynomial& find_polynomial,
                         IFindLiteralValue& find_literal_value,
                         IMakeLiteral& make_literal,
                         std::vector<uint32_t> variables);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
private:
    IFindPolynomial& find_polynomial_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    std::vector<uint32_t> variables_;
};

#define POLY_POINT_EVALUATOR_TEMPLATE \
    template<typename IFindPolynomial, typename IFindLiteralValue, typename IMakeLiteral>
#define POLY_POINT_EVALUATOR poly_point_evaluator<IFindPolynomial, IFindLiteralValue, IMakeLiteral>

POLY_POINT_EVALUATOR_TEMPLATE POLY_POINT_EVALUATOR::poly_point_evaluator(
    IFindPolynomial& find_polynomial,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    std::vector<uint32_t> variables)
    : find_polynomial_(find_polynomial)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
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
    for (size_t index = 0; index < variables_.size(); ++index) {
        const std::optional<integer> coordinate = find_literal_value_.find_value(point->args[index]);
        if (!coordinate.has_value())
            return std::nullopt;
        coordinates.emplace(variables_[index], coordinate.value());
    }
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

#undef POLY_POINT_EVALUATOR
#undef POLY_POINT_EVALUATOR_TEMPLATE

#endif
