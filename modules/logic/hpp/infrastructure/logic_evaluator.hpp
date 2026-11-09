#ifndef LOGIC_EVALUATOR_HPP
#define LOGIC_EVALUATOR_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/logic_symbols.hpp"

// The truth value of a formula at an input `point(v1, ..., vk)` of 0s and 1s, binding the i-th variable to the i-th
// coordinate, as the literal 0 or 1. Also decides a formula used as a region condition: it holds where it is 1. No
// value for anything outside the logic vocabulary, a literal other than 0 or 1, or an unknown variable.
template<typename IFindLiteralValue, typename IMakeLiteral>
struct logic_evaluator {
    logic_evaluator(IFindLiteralValue& find_literal_value,
                    IMakeLiteral& make_literal,
                    logic_symbols symbols,
                    std::vector<uint32_t> variables);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
    std::optional<bool> holds_at(const expr* condition, const expr* input);
private:
    std::optional<bool> find_truth(const expr* e, const std::vector<const expr*>& coordinates);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    logic_symbols symbols_;
    std::vector<uint32_t> variables_;
};

#define LOGIC_EVALUATOR_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral>
#define LOGIC_EVALUATOR          logic_evaluator<IFindLiteralValue, IMakeLiteral>

LOGIC_EVALUATOR_TEMPLATE LOGIC_EVALUATOR::logic_evaluator(
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    logic_symbols symbols,
    std::vector<uint32_t> variables)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , symbols_(symbols)
    , variables_(std::move(variables)) {}

LOGIC_EVALUATOR_TEMPLATE std::optional<const expr*> LOGIC_EVALUATOR::evaluate_at(const expr* e, const expr* input) {
    const std::optional<bool> truth = holds_at(e, input);
    if (!truth.has_value())
        return std::nullopt;
    return make_literal_.make_literal(integer(truth.value() ? 1 : 0));
}

LOGIC_EVALUATOR_TEMPLATE std::optional<bool> LOGIC_EVALUATOR::holds_at(const expr* condition, const expr* input) {
    const expr::functor* point = std::get_if<expr::functor>(&input->content);
    if (point == nullptr || point->args.size() != variables_.size())
        return std::nullopt;
    return find_truth(condition, point->args);
}

LOGIC_EVALUATOR_TEMPLATE std::optional<bool> LOGIC_EVALUATOR::find_truth(
    const expr* e, const std::vector<const expr*>& coordinates) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return std::nullopt;
    if (f->args.empty()) {
        const auto variable = std::find(variables_.begin(), variables_.end(), f->id);
        const expr* constant = variable == variables_.end() ? e : coordinates[variable - variables_.begin()];
        const std::optional<integer> value = find_literal_value_.find_value(constant);
        const bool truth_value = value.has_value() && (value.value() == 0 || value.value() == 1);
        if (!truth_value)
            return std::nullopt;
        return value.value() == 1;
    }
    if (f->id == symbols_.negation && f->args.size() == 1) {
        const std::optional<bool> operand = find_truth(f->args[0], coordinates);
        if (!operand.has_value())
            return std::nullopt;
        return !operand.value();
    }
    const bool connective = f->args.size() == 2
                            && (f->id == symbols_.conjunction || f->id == symbols_.disjunction
                                || f->id == symbols_.exclusion);
    if (!connective)
        return std::nullopt;
    const std::optional<bool> left = find_truth(f->args[0], coordinates);
    if (!left.has_value())
        return std::nullopt;
    const std::optional<bool> right = find_truth(f->args[1], coordinates);
    if (!right.has_value())
        return std::nullopt;
    if (f->id == symbols_.conjunction)
        return left.value() && right.value();
    if (f->id == symbols_.disjunction)
        return left.value() || right.value();
    return left.value() != right.value();
}

#undef LOGIC_EVALUATOR
#undef LOGIC_EVALUATOR_TEMPLATE

#endif
