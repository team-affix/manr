#ifndef LIST_FILTER_HYPOTHESES_HPP
#define LIST_FILTER_HYPOTHESES_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads list-to-list data as `filter(c, #0)`. A point is a filtering when its output is its input with
// every occurrence of some values removed; its kept values are the output's. When every kept value of the data lies
// above every removed one, it proposes `filter(lt(d, e), #0)` with d the largest removed value; when below,
// `filter(lt(e, d), #0)` with d the smallest removed value. Split outer-inner (`docs/decomposing-detection.md`), the
// kept and removed values are also condition data, `point(v) -> 1` and `point(v) -> 0`, handed to the condition
// domain: each condition it proposes becomes `filter(lam(c), #0)` (e.g. `is_prime(#0)`, reduce request #14). Points of
// any other shape are ignored.
template<typename IFindElements,
         typename IFindLiteralValue,
         typename IMakeLiteral,
         typename IMakeFunctor,
         typename IProposeConditions>
struct list_filter_hypotheses {
    list_filter_hypotheses(IFindElements& find_elements,
                           IFindLiteralValue& find_literal_value,
                           IMakeLiteral& make_literal,
                           IMakeFunctor& make_functor,
                           IProposeConditions& propose_conditions,
                           list_symbols symbols,
                           uint32_t less_symbol,
                           uint32_t point_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    std::optional<std::vector<integer>> find_values(const expr* list) const;

    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    IProposeConditions& propose_conditions_;
    list_symbols symbols_;
    uint32_t less_symbol_;
    uint32_t point_symbol_;
    std::vector<const expr*> conditions_;
};

#define LIST_FILTER_HYPOTHESES_TEMPLATE                                                                    \
    template<typename IFindElements, typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor, \
             typename IProposeConditions>
#define LIST_FILTER_HYPOTHESES \
    list_filter_hypotheses<IFindElements, IFindLiteralValue, IMakeLiteral, IMakeFunctor, IProposeConditions>

LIST_FILTER_HYPOTHESES_TEMPLATE LIST_FILTER_HYPOTHESES::list_filter_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    IProposeConditions& propose_conditions,
    list_symbols symbols,
    uint32_t less_symbol,
    uint32_t point_symbol)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , propose_conditions_(propose_conditions)
    , symbols_(symbols)
    , less_symbol_(less_symbol)
    , point_symbol_(point_symbol)
    , conditions_() {}

LIST_FILTER_HYPOTHESES_TEMPLATE void LIST_FILTER_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::set<integer> kept;
    std::set<integer> removed;
    for (const pw_data_point& point : points) {
        const std::optional<std::vector<integer>> inputs = find_values(point.input);
        if (!inputs.has_value())
            continue;
        const std::optional<std::vector<integer>> outputs = find_values(point.output);
        if (!outputs.has_value())
            continue;
        const std::set<integer> kept_here(outputs.value().begin(), outputs.value().end());
        std::vector<integer> filtered;
        for (const integer& input : inputs.value())
            if (kept_here.contains(input))
                filtered.push_back(input);
        if (filtered != outputs.value())
            continue;
        for (const integer& input : inputs.value())
            (kept_here.contains(input) ? kept : removed).insert(input);
    }
    if (kept.empty() || removed.empty())
        return;
    const expr* element = make_functor_.make_functor(symbols_.element, {});
    const expr* list = make_functor_.make_functor(symbols_.input, {});
    std::vector<pw_data_point> labelled;
    labelled.reserve(kept.size() + removed.size());
    const expr* holds = make_literal_.make_literal(integer(1));
    const expr* fails = make_literal_.make_literal(integer(0));
    for (const integer& value : kept) {
        const expr* key = make_functor_.make_functor(point_symbol_, {make_literal_.make_literal(value)});
        labelled.push_back(pw_data_point{.input = key, .output = holds});
    }
    for (const integer& value : removed) {
        const expr* key = make_functor_.make_functor(point_symbol_, {make_literal_.make_literal(value)});
        labelled.push_back(pw_data_point{.input = key, .output = fails});
    }
    conditions_.clear();
    propose_conditions_.propose_hypotheses(labelled, conditions_);
    for (const expr* condition : conditions_) {
        const expr* function = make_functor_.make_functor(symbols_.lambda, {condition});
        out.push_back(make_functor_.make_functor(symbols_.filter, {function, list}));
    }
    const bool above = *removed.rbegin() < *kept.begin();
    if (above) {
        const expr* condition =
            make_functor_.make_functor(less_symbol_, {make_literal_.make_literal(*removed.rbegin()), element});
        const expr* function = make_functor_.make_functor(symbols_.lambda, {condition});
        out.push_back(make_functor_.make_functor(symbols_.filter, {function, list}));
        return;
    }
    const bool below = *kept.rbegin() < *removed.begin();
    if (!below)
        return;
    const expr* condition =
        make_functor_.make_functor(less_symbol_, {element, make_literal_.make_literal(*removed.begin())});
    const expr* function = make_functor_.make_functor(symbols_.lambda, {condition});
    out.push_back(make_functor_.make_functor(symbols_.filter, {function, list}));
}

// The literal values of a list of literals.
LIST_FILTER_HYPOTHESES_TEMPLATE std::optional<std::vector<integer>> LIST_FILTER_HYPOTHESES::find_values(
    const expr* list) const {
    const std::optional<std::vector<const expr*>> elements = find_elements_.find_elements(list);
    if (!elements.has_value())
        return std::nullopt;
    std::vector<integer> values;
    values.reserve(elements.value().size());
    for (const expr* element : elements.value()) {
        const std::optional<integer> value = find_literal_value_.find_value(element);
        if (!value.has_value())
            return std::nullopt;
        values.push_back(value.value());
    }
    return values;
}

#undef LIST_FILTER_HYPOTHESES
#undef LIST_FILTER_HYPOTHESES_TEMPLATE

#endif
