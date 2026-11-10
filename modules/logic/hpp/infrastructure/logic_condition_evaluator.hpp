#ifndef LOGIC_CONDITION_EVALUATOR_HPP
#define LOGIC_CONDITION_EVALUATOR_HPP

#include <cstddef>
#include <optional>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/logic_symbols.hpp"

// Decides a condition built with the logic connectives over another domain's conditions: `and`, `or`, `xor`, `xnor`,
// `nand`, `nor`, `imp`, `not`, `maj` and `mux` combine what their operands decide, and every other condition
// (`lt(0, #0)`, `is_prime(#0)`, a residue) goes to the inner domain. So a filter or a region can take any Boolean function of the
// inner domain's predicates. No answer when an operand has none.
template<typename IHoldsInnerAt>
struct logic_condition_evaluator {
    logic_condition_evaluator(IHoldsInnerAt& holds_inner_at, logic_symbols symbols);
    std::optional<bool> holds_at(const expr* condition, const expr* input);
private:
    IHoldsInnerAt& holds_inner_at_;
    logic_symbols symbols_;
};

#define LOGIC_CONDITION_EVALUATOR_TEMPLATE template<typename IHoldsInnerAt>
#define LOGIC_CONDITION_EVALUATOR          logic_condition_evaluator<IHoldsInnerAt>

LOGIC_CONDITION_EVALUATOR_TEMPLATE LOGIC_CONDITION_EVALUATOR::logic_condition_evaluator(IHoldsInnerAt& holds_inner_at,
                                                                                         logic_symbols symbols)
    : holds_inner_at_(holds_inner_at)
    , symbols_(symbols) {}

LOGIC_CONDITION_EVALUATOR_TEMPLATE std::optional<bool> LOGIC_CONDITION_EVALUATOR::holds_at(const expr* condition,
                                                                                           const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&condition->content);
    if (f == nullptr)
        return std::nullopt;
    if (f->id == symbols_.negation && f->args.size() == 1) {
        const std::optional<bool> operand = holds_at(f->args[0], input);
        if (!operand.has_value())
            return std::nullopt;
        return !operand.value();
    }
    if (f->id == symbols_.multiplexer && f->args.size() == 3) {
        const std::optional<bool> select = holds_at(f->args[0], input);
        if (!select.has_value())
            return std::nullopt;
        return holds_at(f->args[select.value() ? 1 : 2], input);
    }
    if (f->id == symbols_.majority && f->args.size() == 3) {
        size_t ones = 0;
        for (const expr* arg : f->args) {
            const std::optional<bool> operand = holds_at(arg, input);
            if (!operand.has_value())
                return std::nullopt;
            ones += operand.value() ? 1 : 0;
        }
        return ones >= 2;
    }
    const bool connective = f->args.size() == 2
                            && (f->id == symbols_.conjunction || f->id == symbols_.disjunction
                                || f->id == symbols_.exclusion || f->id == symbols_.equivalence
                                || f->id == symbols_.alternative_denial || f->id == symbols_.joint_denial
                                || f->id == symbols_.implication);
    if (!connective)
        return holds_inner_at_.holds_at(condition, input);
    const std::optional<bool> left = holds_at(f->args[0], input);
    if (!left.has_value())
        return std::nullopt;
    const std::optional<bool> right = holds_at(f->args[1], input);
    if (!right.has_value())
        return std::nullopt;
    if (f->id == symbols_.conjunction)
        return left.value() && right.value();
    if (f->id == symbols_.disjunction)
        return left.value() || right.value();
    if (f->id == symbols_.exclusion)
        return left.value() != right.value();
    if (f->id == symbols_.equivalence)
        return left.value() == right.value();
    if (f->id == symbols_.alternative_denial)
        return !(left.value() && right.value());
    if (f->id == symbols_.joint_denial)
        return !(left.value() || right.value());
    return !left.value() || right.value();
}

#undef LOGIC_CONDITION_EVALUATOR
#undef LOGIC_CONDITION_EVALUATOR_TEMPLATE

#endif
