#ifndef ARITH_EVALUATOR_HPP
#define ARITH_EVALUATOR_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include "value_objects/arith_symbols.hpp"
#include "value_objects/expr.hpp"

// Evaluates an arithmetic expression with the input functor bound to a literal. Anything outside the vocabulary,
// extension terms included, has no value.
template<typename IFindLiteralValue, typename IMakeLiteral>
struct arith_evaluator {
    arith_evaluator(IFindLiteralValue& find_literal_value, IMakeLiteral& make_literal, arith_symbols symbols);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
private:
    std::optional<int64_t> find_value_at(const expr* e, int64_t input) const;

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    arith_symbols symbols_;
};

#define ARITH_EVALUATOR_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral>
#define ARITH_EVALUATOR          arith_evaluator<IFindLiteralValue, IMakeLiteral>

ARITH_EVALUATOR_TEMPLATE ARITH_EVALUATOR::arith_evaluator(
    IFindLiteralValue& find_literal_value, IMakeLiteral& make_literal, arith_symbols symbols)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , symbols_(symbols) {}

ARITH_EVALUATOR_TEMPLATE std::optional<const expr*> ARITH_EVALUATOR::evaluate_at(const expr* e, const expr* input) {
    const std::optional<int64_t> input_value = find_literal_value_.find_value(input);
    if (!input_value.has_value())
        return std::nullopt;
    const std::optional<int64_t> value = find_value_at(e, input_value.value());
    if (!value.has_value())
        return std::nullopt;
    return make_literal_.make_literal(value.value());
}

ARITH_EVALUATOR_TEMPLATE std::optional<int64_t> ARITH_EVALUATOR::find_value_at(const expr* e, int64_t input) const {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return std::nullopt;
    if (f->id == symbols_.input && f->args.empty())
        return input;
    if (f->args.empty())
        return find_literal_value_.find_value(e);
    const bool binary = f->args.size() == 2;
    if (!binary)
        return std::nullopt;
    const std::optional<int64_t> lhs = find_value_at(f->args[0], input);
    const std::optional<int64_t> rhs = find_value_at(f->args[1], input);
    if (!lhs.has_value() || !rhs.has_value())
        return std::nullopt;
    if (f->id == symbols_.add)
        return lhs.value() + rhs.value();
    if (f->id == symbols_.sub)
        return lhs.value() - rhs.value();
    if (f->id == symbols_.mul)
        return lhs.value() * rhs.value();
    return std::nullopt;
}

#undef ARITH_EVALUATOR
#undef ARITH_EVALUATOR_TEMPLATE

#endif
