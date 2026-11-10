#ifndef POLY_CONDITION_EVALUATOR_HPP
#define POLY_CONDITION_EVALUATOR_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"

// Decides a region condition at an input: `lt(a, b)` holds where a's value is less than b's, `eq(a, b)` where they
// are equal. A side may be `mod(a, m)` for a positive literal m: a's value modulo m, from 0 to m - 1 (so
// `eq(mod(x, 2), 0)` holds where x is even, negative or not). Undecided (no value) for any other condition, or where
// either side has no value.
template<typename IEvaluateAt, typename IFindLiteralValue>
struct poly_condition_evaluator {
    poly_condition_evaluator(IEvaluateAt& evaluate_at,
                             IFindLiteralValue& find_literal_value,
                             uint32_t less_symbol,
                             uint32_t equal_symbol,
                             uint32_t modulo_symbol);
    std::optional<bool> holds_at(const expr* condition, const expr* input);
private:
    std::optional<integer> find_value(const expr* e, const expr* input);

    IEvaluateAt& evaluate_at_;
    IFindLiteralValue& find_literal_value_;
    uint32_t less_symbol_;
    uint32_t equal_symbol_;
    uint32_t modulo_symbol_;
};

#define POLY_CONDITION_EVALUATOR_TEMPLATE template<typename IEvaluateAt, typename IFindLiteralValue>
#define POLY_CONDITION_EVALUATOR          poly_condition_evaluator<IEvaluateAt, IFindLiteralValue>

POLY_CONDITION_EVALUATOR_TEMPLATE POLY_CONDITION_EVALUATOR::poly_condition_evaluator(
    IEvaluateAt& evaluate_at,
    IFindLiteralValue& find_literal_value,
    uint32_t less_symbol,
    uint32_t equal_symbol,
    uint32_t modulo_symbol)
    : evaluate_at_(evaluate_at)
    , find_literal_value_(find_literal_value)
    , less_symbol_(less_symbol)
    , equal_symbol_(equal_symbol)
    , modulo_symbol_(modulo_symbol) {}

POLY_CONDITION_EVALUATOR_TEMPLATE std::optional<bool> POLY_CONDITION_EVALUATOR::holds_at(
    const expr* condition, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&condition->content);
    const bool comparison = f != nullptr && f->args.size() == 2 && (f->id == less_symbol_ || f->id == equal_symbol_);
    if (!comparison)
        return std::nullopt;
    const std::optional<integer> left = find_value(f->args[0], input);
    if (!left.has_value())
        return std::nullopt;
    const std::optional<integer> right = find_value(f->args[1], input);
    if (!right.has_value())
        return std::nullopt;
    if (f->id == equal_symbol_)
        return left.value() == right.value();
    return left.value() < right.value();
}

POLY_CONDITION_EVALUATOR_TEMPLATE std::optional<integer> POLY_CONDITION_EVALUATOR::find_value(
    const expr* e, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const bool modulo = f != nullptr && f->id == modulo_symbol_ && f->args.size() == 2;
    if (modulo) {
        const std::optional<integer> dividend = find_value(f->args[0], input);
        const std::optional<integer> divisor = find_literal_value_.find_value(f->args[1]);
        if (!dividend.has_value() || !divisor.has_value() || divisor.value() <= 0)
            return std::nullopt;
        const integer remainder = dividend.value() % divisor.value();
        return remainder < 0 ? integer(remainder + divisor.value()) : remainder;
    }
    const std::optional<const expr*> value = evaluate_at_.evaluate_at(e, input);
    if (!value.has_value())
        return std::nullopt;
    return find_literal_value_.find_value(value.value());
}

#undef POLY_CONDITION_EVALUATOR
#undef POLY_CONDITION_EVALUATOR_TEMPLATE

#endif
