#ifndef ABS_APPLYING_EVALUATOR_HPP
#define ABS_APPLYING_EVALUATOR_HPP

#include <optional>
#include <variant>
#include "value_objects/expr.hpp"

// The value of an expression at an input, where a function value is applied to the input: a `lam` or `lams` (any
// chain of them) is unwrapped and its body evaluated by the domain (through its piecewise evaluator, so a body may
// itself hold regions) at the input, which binds the body's indices to
// the input's coordinates (the first `#(k-1)`, the last `#0`). Anything else is evaluated by the domain as it is. So
// a piecewise's catch-all can be a closed function of the piecewise's inputs.
template<typename ICountBinders, typename IEvaluateAt>
struct abs_applying_evaluator {
    abs_applying_evaluator(ICountBinders& count_binders, IEvaluateAt& evaluate_at);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
private:
    ICountBinders& count_binders_;
    IEvaluateAt& evaluate_at_;
};

#define ABS_APPLYING_EVALUATOR_TEMPLATE template<typename ICountBinders, typename IEvaluateAt>
#define ABS_APPLYING_EVALUATOR          abs_applying_evaluator<ICountBinders, IEvaluateAt>

ABS_APPLYING_EVALUATOR_TEMPLATE ABS_APPLYING_EVALUATOR::abs_applying_evaluator(
    ICountBinders& count_binders, IEvaluateAt& evaluate_at)
    : count_binders_(count_binders)
    , evaluate_at_(evaluate_at) {}

ABS_APPLYING_EVALUATOR_TEMPLATE std::optional<const expr*> ABS_APPLYING_EVALUATOR::evaluate_at(
    const expr* e, const expr* input) {
    const expr* body = e;
    while (count_binders_.count_binders(body) > 0)
        body = std::get<expr::functor>(body->content).args.back();
    return evaluate_at_.evaluate_at(body, input);
}

#undef ABS_APPLYING_EVALUATOR
#undef ABS_APPLYING_EVALUATOR_TEMPLATE

#endif
