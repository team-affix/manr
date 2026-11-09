#ifndef SEQ_EVALUATOR_HPP
#define SEQ_EVALUATOR_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_symbols.hpp"
#include "value_objects/seq_symbols.hpp"

constexpr size_t k_seq_default_max_index = 100000;

// The value of a sequence expression at an input, as an interned literal: `rec` runs its recurrence up to the index,
// unwrapping its step function's k + 1 `lam` binders and handing the body to the step domain at
// `point(index, a(j-1), a(j-2), ...)`, one coordinate per `previous` index (`#0`, `#1`, ... in that order); `exp`
// raises its base. Their arguments are evaluated here, so they may be any expression of the outer domain. Anything
// else goes to the outer domain (`evaluate_outer_at`), and arithmetic it cannot evaluate (`add`, `sub`, `mul`, `pow`
// over a recurrence or a power, as in `5 * exp(3, n)`) is evaluated here from its arguments' values. No value at a
// negative index or one above `max_index`, or where a domain has none.
template<typename IEvaluateOuterAt, typename IEvaluateStepAt, typename IFindLiteralValue, typename IMakeLiteral,
         typename IMakeFunctor>
struct seq_evaluator {
    seq_evaluator(IEvaluateOuterAt& evaluate_outer_at,
                  IEvaluateStepAt& evaluate_step_at,
                  IFindLiteralValue& find_literal_value,
                  IMakeLiteral& make_literal,
                  IMakeFunctor& make_functor,
                  seq_symbols symbols,
                  poly_symbols arithmetic_symbols,
                  uint32_t point_symbol,
                  size_t max_index);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
private:
    std::optional<integer> find_integer(const expr* e, const expr* input);
    std::optional<const expr*> evaluate_arithmetic(const expr::functor& operation, const expr* input);
    std::optional<const expr*> evaluate_recurrence(const expr::functor& recurrence, size_t last, const expr* input);

    IEvaluateOuterAt& evaluate_outer_at_;
    IEvaluateStepAt& evaluate_step_at_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    seq_symbols symbols_;
    poly_symbols arithmetic_symbols_;
    uint32_t point_symbol_;
    size_t max_index_;
};

#define SEQ_EVALUATOR_TEMPLATE                                                                       \
    template<typename IEvaluateOuterAt, typename IEvaluateStepAt, typename IFindLiteralValue, \
             typename IMakeLiteral, typename IMakeFunctor>
#define SEQ_EVALUATOR \
    seq_evaluator<IEvaluateOuterAt, IEvaluateStepAt, IFindLiteralValue, IMakeLiteral, IMakeFunctor>

SEQ_EVALUATOR_TEMPLATE SEQ_EVALUATOR::seq_evaluator(
    IEvaluateOuterAt& evaluate_outer_at,
    IEvaluateStepAt& evaluate_step_at,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    seq_symbols symbols,
    poly_symbols arithmetic_symbols,
    uint32_t point_symbol,
    size_t max_index)
    : evaluate_outer_at_(evaluate_outer_at)
    , evaluate_step_at_(evaluate_step_at)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , arithmetic_symbols_(arithmetic_symbols)
    , point_symbol_(point_symbol)
    , max_index_(max_index) {}

SEQ_EVALUATOR_TEMPLATE std::optional<const expr*> SEQ_EVALUATOR::evaluate_at(const expr* e, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    const bool recurrence = f != nullptr && f->id == symbols_.recurrence && f->args.size() >= 3
                            && f->args.size() - 2 <= symbols_.previous.size();
    const bool power = f != nullptr && f->id == symbols_.power && f->args.size() == 2;
    if (!recurrence && !power) {
        const std::optional<const expr*> outer = evaluate_outer_at_.evaluate_at(e, input);
        if (outer.has_value() || f == nullptr)
            return outer;
        return evaluate_arithmetic(*f, input);
    }
    const std::optional<integer> index = find_integer(f->args.back(), input);
    if (!index.has_value() || index.value() < 0 || index.value() > max_index_)
        return std::nullopt;
    const size_t last = index.value().template convert_to<size_t>();
    if (recurrence)
        return evaluate_recurrence(*f, last, input);
    const std::optional<integer> base = find_integer(f->args[0], input);
    if (!base.has_value())
        return std::nullopt;
    return make_literal_.make_literal(boost::multiprecision::pow(base.value(), static_cast<unsigned>(last)));
}

SEQ_EVALUATOR_TEMPLATE std::optional<integer> SEQ_EVALUATOR::find_integer(const expr* e, const expr* input) {
    const std::optional<const expr*> value = evaluate_at(e, input);
    if (!value.has_value())
        return std::nullopt;
    return find_literal_value_.find_value(value.value());
}

SEQ_EVALUATOR_TEMPLATE std::optional<const expr*> SEQ_EVALUATOR::evaluate_arithmetic(
    const expr::functor& operation, const expr* input) {
    const bool binary = operation.args.size() == 2;
    const bool arithmetic = operation.id == arithmetic_symbols_.add || operation.id == arithmetic_symbols_.sub
                            || operation.id == arithmetic_symbols_.mul || operation.id == arithmetic_symbols_.pow;
    if (!binary || !arithmetic)
        return std::nullopt;
    const std::optional<integer> left = find_integer(operation.args[0], input);
    if (!left.has_value())
        return std::nullopt;
    if (operation.id == arithmetic_symbols_.pow) {
        const std::optional<integer> exponent = find_literal_value_.find_value(operation.args[1]);
        if (!exponent.has_value() || exponent.value() < 0 || exponent.value() > max_index_)
            return std::nullopt;
        return make_literal_.make_literal(
            boost::multiprecision::pow(left.value(), exponent.value().template convert_to<unsigned>()));
    }
    const std::optional<integer> right = find_integer(operation.args[1], input);
    if (!right.has_value())
        return std::nullopt;
    if (operation.id == arithmetic_symbols_.add)
        return make_literal_.make_literal(integer(left.value() + right.value()));
    if (operation.id == arithmetic_symbols_.sub)
        return make_literal_.make_literal(integer(left.value() - right.value()));
    return make_literal_.make_literal(integer(left.value() * right.value()));
}

SEQ_EVALUATOR_TEMPLATE std::optional<const expr*> SEQ_EVALUATOR::evaluate_recurrence(
    const expr::functor& recurrence, size_t last, const expr* input) {
    const size_t order = recurrence.args.size() - 2;
    // the step's body, under one `lam` for the index and one per previous term
    const expr* step = recurrence.args[0];
    for (size_t binder = 0; binder <= order; ++binder) {
        const expr::functor* function = std::get_if<expr::functor>(&step->content);
        if (function == nullptr || function->id != symbols_.lambda || function->args.size() != 1)
            return std::nullopt;
        step = function->args[0];
    }
    std::vector<integer> terms;
    terms.reserve(last + 1);
    for (size_t position = 0; position < order; ++position) {
        const std::optional<integer> base = find_integer(recurrence.args[1 + position], input);
        if (!base.has_value())
            return std::nullopt;
        terms.push_back(base.value());
    }
    for (size_t position = order; position <= last; ++position) {
        std::vector<const expr*> coordinates{make_literal_.make_literal(integer(position))};
        for (size_t back = 1; back <= symbols_.previous.size(); ++back) {
            const bool started = back <= position;
            coordinates.push_back(make_literal_.make_literal(started ? terms[position - back] : integer(0)));
        }
        const std::optional<const expr*> value =
            evaluate_step_at_.evaluate_at(step, make_functor_.make_functor(point_symbol_, coordinates));
        if (!value.has_value())
            return std::nullopt;
        const std::optional<integer> term = find_literal_value_.find_value(value.value());
        if (!term.has_value())
            return std::nullopt;
        terms.push_back(term.value());
    }
    return make_literal_.make_literal(terms[last]);
}

#undef SEQ_EVALUATOR
#undef SEQ_EVALUATOR_TEMPLATE

#endif
