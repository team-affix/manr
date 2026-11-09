#ifndef ABS_EVALUATOR_HPP
#define ABS_EVALUATOR_HPP

#include <optional>
#include "value_objects/expr.hpp"

// The value of an expression with helpers at an input: the expression is reduced to one without them (`let` and
// applied lambdas beta-reduced), then handed to the domain's evaluator. So any domain evaluates helpers without
// knowing about them. No value when the reduction does not finish or the domain has none.
template<typename IReduce, typename IEvaluateAt>
struct abs_evaluator {
    abs_evaluator(IReduce& reduce, IEvaluateAt& evaluate_at);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
private:
    IReduce& reduce_;
    IEvaluateAt& evaluate_at_;
};

#define ABS_EVALUATOR_TEMPLATE template<typename IReduce, typename IEvaluateAt>
#define ABS_EVALUATOR          abs_evaluator<IReduce, IEvaluateAt>

ABS_EVALUATOR_TEMPLATE ABS_EVALUATOR::abs_evaluator(IReduce& reduce, IEvaluateAt& evaluate_at)
    : reduce_(reduce)
    , evaluate_at_(evaluate_at) {}

ABS_EVALUATOR_TEMPLATE std::optional<const expr*> ABS_EVALUATOR::evaluate_at(const expr* e, const expr* input) {
    const std::optional<const expr*> reduced = reduce_.reduce(e);
    if (!reduced.has_value())
        return std::nullopt;
    return evaluate_at_.evaluate_at(reduced.value(), input);
}

#undef ABS_EVALUATOR
#undef ABS_EVALUATOR_TEMPLATE

#endif
