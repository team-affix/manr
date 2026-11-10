#ifndef PW_EVALUATOR_HPP
#define PW_EVALUATOR_HPP

#include <cstddef>
#include <optional>
#include <variant>
#include "value_objects/expr.hpp"
#include "value_objects/pw_symbols.hpp"

// The value of an expression at an input, through any piecewise in it: the first branch that applies gives the
// value (`case(key, value)` when `key` is the input, `when(condition, value)` when the domain says the condition
// holds there), else the otherwise. Anything that is not a piecewise is evaluated by the domain. A case's label the
// domain cannot evaluate (one holding holes) is its own value. No value when the domain cannot evaluate a model, or
// cannot decide a condition it reaches.
template<typename IEvaluateAt, typename IHoldsAt>
struct pw_evaluator {
    pw_evaluator(IEvaluateAt& evaluate_at, IHoldsAt& holds_at, pw_symbols symbols);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
private:
    IEvaluateAt& evaluate_at_;
    IHoldsAt& holds_at_;
    pw_symbols symbols_;
};

#define PW_EVALUATOR_TEMPLATE template<typename IEvaluateAt, typename IHoldsAt>
#define PW_EVALUATOR          pw_evaluator<IEvaluateAt, IHoldsAt>

PW_EVALUATOR_TEMPLATE PW_EVALUATOR::pw_evaluator(IEvaluateAt& evaluate_at, IHoldsAt& holds_at, pw_symbols symbols)
    : evaluate_at_(evaluate_at)
    , holds_at_(holds_at)
    , symbols_(symbols) {}

PW_EVALUATOR_TEMPLATE std::optional<const expr*> PW_EVALUATOR::evaluate_at(const expr* e, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const bool piecewise = f != nullptr && f->id == symbols_.piecewise;
    if (!piecewise)
        return evaluate_at_.evaluate_at(e, input);
    for (size_t index = 0; index + 1 < f->args.size(); ++index) {
        const expr::functor& branch = std::get<expr::functor>(f->args[index]->content);
        if (branch.id == symbols_.branch) {
            if (branch.args[0] != input)
                continue;
            // a case's label is its output: the domain evaluates it (a label may be a model), or it stands as is
            const std::optional<const expr*> label = evaluate_at(branch.args[1], input);
            return label.has_value() ? label : std::optional<const expr*>(branch.args[1]);
        }
        if (branch.id != symbols_.region)
            return std::nullopt;
        const std::optional<bool> holds = holds_at_.holds_at(branch.args[0], input);
        if (!holds.has_value())
            return std::nullopt;
        if (!holds.value())
            continue;
        return evaluate_at(branch.args[1], input);
    }
    return evaluate_at(f->args.back(), input);
}

#undef PW_EVALUATOR
#undef PW_EVALUATOR_TEMPLATE

#endif
