#ifndef POLY_CONDITION_EVALUATOR_HPP
#define POLY_CONDITION_EVALUATOR_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

// Decides a region condition at an input: `lt(a, b)` holds where a's value is less than b's. Undecided (no value)
// for any other condition, or where either side has no value.
template<typename IEvaluateAt, typename IFindLiteralValue>
struct poly_condition_evaluator {
    poly_condition_evaluator(IEvaluateAt& evaluate_at, IFindLiteralValue& find_literal_value, uint32_t less_symbol);
    std::optional<bool> holds_at(const expr* condition, const expr* input);
private:
    IEvaluateAt& evaluate_at_;
    IFindLiteralValue& find_literal_value_;
    uint32_t less_symbol_;
};

#define POLY_CONDITION_EVALUATOR_TEMPLATE template<typename IEvaluateAt, typename IFindLiteralValue>
#define POLY_CONDITION_EVALUATOR          poly_condition_evaluator<IEvaluateAt, IFindLiteralValue>

POLY_CONDITION_EVALUATOR_TEMPLATE POLY_CONDITION_EVALUATOR::poly_condition_evaluator(
    IEvaluateAt& evaluate_at, IFindLiteralValue& find_literal_value, uint32_t less_symbol)
    : evaluate_at_(evaluate_at)
    , find_literal_value_(find_literal_value)
    , less_symbol_(less_symbol) {}

POLY_CONDITION_EVALUATOR_TEMPLATE std::optional<bool> POLY_CONDITION_EVALUATOR::holds_at(
    const expr* condition, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&condition->content);
    if (f == nullptr || f->id != less_symbol_ || f->args.size() != 2)
        return std::nullopt;
    const std::optional<const expr*> left = evaluate_at_.evaluate_at(f->args[0], input);
    if (!left.has_value())
        return std::nullopt;
    const std::optional<const expr*> right = evaluate_at_.evaluate_at(f->args[1], input);
    if (!right.has_value())
        return std::nullopt;
    const std::optional<integer> left_value = find_literal_value_.find_value(left.value());
    const std::optional<integer> right_value = find_literal_value_.find_value(right.value());
    if (!left_value.has_value() || !right_value.has_value())
        return std::nullopt;
    return left_value.value() < right_value.value();
}

#undef POLY_CONDITION_EVALUATOR
#undef POLY_CONDITION_EVALUATOR_TEMPLATE

#endif
