#ifndef LIST_EVALUATOR_HPP
#define LIST_EVALUATOR_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"

// The value of a list program at an input list, as an interned value: a list (`cons`/`nil` chain) or an element. `xs`
// is the input; literals and `nil` are themselves; `cons` builds a list from the values of its arguments; `map`,
// `filter` and `fold` run over the value of their list argument, handing their bodies to the element domain at
// `point(e)` for map and filter and `point(h, acc)` for fold. Also decides `lt(a, b)` between two values that are
// literals, for regions over lists. No value for anything else, or where the element domain has none.
template<typename IFindElements,
         typename IMakeList,
         typename IMakeFunctor,
         typename IFindLiteralValue,
         typename IEvaluateElementAt,
         typename IHoldsAtElement,
         typename IEvaluateStepAt>
struct list_evaluator {
    list_evaluator(IFindElements& find_elements,
                   IMakeList& make_list,
                   IMakeFunctor& make_functor,
                   IFindLiteralValue& find_literal_value,
                   IEvaluateElementAt& evaluate_element_at,
                   IHoldsAtElement& holds_at_element,
                   IEvaluateStepAt& evaluate_step_at,
                   list_symbols symbols,
                   uint32_t point_symbol,
                   uint32_t less_symbol);
    std::optional<const expr*> evaluate_at(const expr* e, const expr* input);
    std::optional<bool> holds_at(const expr* condition, const expr* input);
private:
    std::optional<const expr*> evaluate_map(const expr* body, const std::vector<const expr*>& elements);
    std::optional<const expr*> evaluate_filter(const expr* condition, const std::vector<const expr*>& elements);
    std::optional<const expr*> evaluate_fold(const expr* step, const expr* base,
                                             const std::vector<const expr*>& elements);

    IFindElements& find_elements_;
    IMakeList& make_list_;
    IMakeFunctor& make_functor_;
    IFindLiteralValue& find_literal_value_;
    IEvaluateElementAt& evaluate_element_at_;
    IHoldsAtElement& holds_at_element_;
    IEvaluateStepAt& evaluate_step_at_;
    list_symbols symbols_;
    uint32_t point_symbol_;
    uint32_t less_symbol_;
};

#define LIST_EVALUATOR_TEMPLATE                                                                               \
    template<typename IFindElements, typename IMakeList, typename IMakeFunctor, typename IFindLiteralValue, \
             typename IEvaluateElementAt, typename IHoldsAtElement, typename IEvaluateStepAt>
#define LIST_EVALUATOR                                                                                       \
    list_evaluator<IFindElements, IMakeList, IMakeFunctor, IFindLiteralValue, IEvaluateElementAt, IHoldsAtElement, \
                   IEvaluateStepAt>

LIST_EVALUATOR_TEMPLATE LIST_EVALUATOR::list_evaluator(
    IFindElements& find_elements,
    IMakeList& make_list,
    IMakeFunctor& make_functor,
    IFindLiteralValue& find_literal_value,
    IEvaluateElementAt& evaluate_element_at,
    IHoldsAtElement& holds_at_element,
    IEvaluateStepAt& evaluate_step_at,
    list_symbols symbols,
    uint32_t point_symbol,
    uint32_t less_symbol)
    : find_elements_(find_elements)
    , make_list_(make_list)
    , make_functor_(make_functor)
    , find_literal_value_(find_literal_value)
    , evaluate_element_at_(evaluate_element_at)
    , holds_at_element_(holds_at_element)
    , evaluate_step_at_(evaluate_step_at)
    , symbols_(symbols)
    , point_symbol_(point_symbol)
    , less_symbol_(less_symbol) {}

LIST_EVALUATOR_TEMPLATE std::optional<const expr*> LIST_EVALUATOR::evaluate_at(const expr* e, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&e->content);
    if (f == nullptr)
        return std::nullopt;
    if (f->args.empty()) {
        if (f->id == symbols_.input)
            return input;
        const bool constant = f->id == symbols_.nil || find_literal_value_.find_value(e).has_value();
        if (!constant)
            return std::nullopt;
        return e;
    }
    if (f->id == symbols_.cons && f->args.size() == 2) {
        const std::optional<const expr*> head = evaluate_at(f->args[0], input);
        if (!head.has_value())
            return std::nullopt;
        const std::optional<const expr*> tail = evaluate_at(f->args[1], input);
        if (!tail.has_value())
            return std::nullopt;
        return make_functor_.make_functor(symbols_.cons, {head.value(), tail.value()});
    }
    const bool program = (f->id == symbols_.map && f->args.size() == 2)
                         || (f->id == symbols_.filter && f->args.size() == 2)
                         || (f->id == symbols_.fold && f->args.size() == 3);
    if (!program)
        return std::nullopt;
    const std::optional<const expr*> list = evaluate_at(f->args.back(), input);
    if (!list.has_value())
        return std::nullopt;
    const std::optional<std::vector<const expr*>> elements = find_elements_.find_elements(list.value());
    if (!elements.has_value())
        return std::nullopt;
    if (f->id == symbols_.map)
        return evaluate_map(f->args[0], elements.value());
    if (f->id == symbols_.filter)
        return evaluate_filter(f->args[0], elements.value());
    const std::optional<const expr*> base = evaluate_at(f->args[1], input);
    if (!base.has_value())
        return std::nullopt;
    return evaluate_fold(f->args[0], base.value(), elements.value());
}

LIST_EVALUATOR_TEMPLATE std::optional<bool> LIST_EVALUATOR::holds_at(const expr* condition, const expr* input) {
    const expr::functor* f = std::get_if<expr::functor>(&condition->content);
    if (f == nullptr || f->id != less_symbol_ || f->args.size() != 2)
        return std::nullopt;
    const std::optional<const expr*> left = evaluate_at(f->args[0], input);
    if (!left.has_value())
        return std::nullopt;
    const std::optional<const expr*> right = evaluate_at(f->args[1], input);
    if (!right.has_value())
        return std::nullopt;
    const std::optional<integer> left_value = find_literal_value_.find_value(left.value());
    const std::optional<integer> right_value = find_literal_value_.find_value(right.value());
    if (!left_value.has_value() || !right_value.has_value())
        return std::nullopt;
    return left_value.value() < right_value.value();
}

LIST_EVALUATOR_TEMPLATE std::optional<const expr*> LIST_EVALUATOR::evaluate_map(
    const expr* body, const std::vector<const expr*>& elements) {
    std::vector<const expr*> mapped;
    mapped.reserve(elements.size());
    for (const expr* element : elements) {
        const std::optional<const expr*> value =
            evaluate_element_at_.evaluate_at(body, make_functor_.make_functor(point_symbol_, {element}));
        if (!value.has_value())
            return std::nullopt;
        mapped.push_back(value.value());
    }
    return make_list_.make_list(mapped);
}

LIST_EVALUATOR_TEMPLATE std::optional<const expr*> LIST_EVALUATOR::evaluate_filter(
    const expr* condition, const std::vector<const expr*>& elements) {
    std::vector<const expr*> kept;
    for (const expr* element : elements) {
        const std::optional<bool> holds =
            holds_at_element_.holds_at(condition, make_functor_.make_functor(point_symbol_, {element}));
        if (!holds.has_value())
            return std::nullopt;
        if (!holds.value())
            continue;
        kept.push_back(element);
    }
    return make_list_.make_list(kept);
}

LIST_EVALUATOR_TEMPLATE std::optional<const expr*> LIST_EVALUATOR::evaluate_fold(
    const expr* step, const expr* base, const std::vector<const expr*>& elements) {
    const expr* accumulated = base;
    for (auto element = elements.rbegin(); element != elements.rend(); ++element) {
        const std::optional<const expr*> value =
            evaluate_step_at_.evaluate_at(step, make_functor_.make_functor(point_symbol_, {*element, accumulated}));
        if (!value.has_value())
            return std::nullopt;
        accumulated = value.value();
    }
    return accumulated;
}

#undef LIST_EVALUATOR
#undef LIST_EVALUATOR_TEMPLATE

#endif
