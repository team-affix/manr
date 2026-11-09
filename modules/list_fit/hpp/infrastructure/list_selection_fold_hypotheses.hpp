#ifndef LIST_SELECTION_FOLD_HYPOTHESES_HPP
#define LIST_SELECTION_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

// A detector that reads list-to-integer data as the largest (or smallest) value of an unknown inner function of the
// elements: `fold(lam(lam(pw(when(lt(#1, g(#0)), g(#0)), #1))), b, xs)`, or with `lt(g(#0), #1)` (the head is `#0`,
// the accumulator `#1`). It is the outer-inner split
// with the outer combiner `max` (`docs/decomposing-detection.md`); `list_extremum_fold_hypotheses` is its specific case
// `g(h) = h`, kept beside it. Each output is `g` of one of its list's elements, so for the largest, `g(v)` is at most
// the smallest output of any list holding `v`: that bound is `g`'s value wherever `v` attains a maximum, and the
// largest bound in each list reproduces its output. A direction is refuted when the bounds reproduce no more than half
// of the lists. A list whose output is the bound of exactly one of its values pins `g` there; those values go to the
// element domain, and each body it proposes, in `#0`, is `g` in the step as it stands, since the head is `#0` too (a
// constant body is left to the constant detector). The base is the empty list's output, else 0 when no output lies
// below it (above, for the smallest), else the lowest (highest) output. `g` that is the identity
// on every pinned value is left to the extremum detector.
template<typename IFindElements,
         typename IFindLiteralValue,
         typename IConvertSamples,
         typename IProposeElementHypotheses,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct list_selection_fold_hypotheses {
    list_selection_fold_hypotheses(IFindElements& find_elements,
                                   IFindLiteralValue& find_literal_value,
                                   IConvertSamples& convert_samples,
                                   IProposeElementHypotheses& propose_element_hypotheses,
                                   IMakeLiteral& make_literal,
                                   IMakeFunctor& make_functor,
                                   list_symbols symbols,
                                   pw_symbols piecewise_symbols,
                                   uint32_t less_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    IConvertSamples& convert_samples_;
    IProposeElementHypotheses& propose_element_hypotheses_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    pw_symbols piecewise_symbols_;
    uint32_t less_symbol_;
    std::vector<const expr*> bodies_;
};

#define LIST_SELECTION_FOLD_HYPOTHESES_TEMPLATE                                                       \
    template<typename IFindElements, typename IFindLiteralValue, typename IConvertSamples,          \
             typename IProposeElementHypotheses, typename IMakeLiteral, typename IMakeFunctor>
#define LIST_SELECTION_FOLD_HYPOTHESES                                                                    \
    list_selection_fold_hypotheses<IFindElements, IFindLiteralValue, IConvertSamples, IProposeElementHypotheses, \
                                   IMakeLiteral, IMakeFunctor>

LIST_SELECTION_FOLD_HYPOTHESES_TEMPLATE LIST_SELECTION_FOLD_HYPOTHESES::list_selection_fold_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    IConvertSamples& convert_samples,
    IProposeElementHypotheses& propose_element_hypotheses,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    pw_symbols piecewise_symbols,
    uint32_t less_symbol)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , convert_samples_(convert_samples)
    , propose_element_hypotheses_(propose_element_hypotheses)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , piecewise_symbols_(piecewise_symbols)
    , less_symbol_(less_symbol)
    , bodies_() {}

LIST_SELECTION_FOLD_HYPOTHESES_TEMPLATE void LIST_SELECTION_FOLD_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<std::vector<integer>> lists;
    std::vector<integer> outputs;
    std::optional<integer> empty_output;
    for (const pw_data_point& point : points) {
        const std::optional<integer> output = find_literal_value_.find_value(point.output);
        if (!output.has_value())
            continue;
        const std::optional<std::vector<const expr*>> elements = find_elements_.find_elements(point.input);
        if (!elements.has_value())
            continue;
        std::vector<integer> list;
        for (const expr* element : elements.value()) {
            const std::optional<integer> value = find_literal_value_.find_value(element);
            if (!value.has_value())
                break;
            list.push_back(value.value());
        }
        if (list.size() != elements.value().size())
            continue;
        if (list.empty()) {
            if (!empty_output.has_value())
                empty_output = output.value();
            continue;
        }
        lists.push_back(std::move(list));
        outputs.push_back(output.value());
    }
    const size_t needed = std::max<size_t>(lists.size() / 2, 1);
    if (lists.size() <= needed)
        return;
    const expr* accumulator = make_functor_.make_functor(symbols_.accumulator, {});
    const expr* input = make_functor_.make_functor(symbols_.input, {});
    for (const bool largest : {true, false}) {
        // each value's bound: the tightest output of the lists holding it
        std::map<integer, integer> bounds;
        for (size_t index = 0; index < lists.size(); ++index)
            for (const integer& value : lists[index]) {
                const auto [bound, inserted] = bounds.emplace(value, outputs[index]);
                const bool tighter = largest ? outputs[index] < bound->second : bound->second < outputs[index];
                if (!inserted && tighter)
                    bound->second = outputs[index];
            }
        size_t reproduced = 0;
        std::map<integer, integer> pinned;
        for (size_t index = 0; index < lists.size(); ++index) {
            std::set<integer> attaining;
            for (const integer& value : lists[index])
                if (bounds.at(value) == outputs[index])
                    attaining.insert(value);
            if (attaining.empty())
                continue;
            ++reproduced;
            if (attaining.size() == 1)
                pinned.emplace(*attaining.begin(), outputs[index]);
        }
        if (reproduced <= needed || pinned.size() < 2)
            continue;
        bool identity = true;
        for (const auto& [value, image] : pinned)
            identity = identity && value == image;
        if (identity)
            continue;
        const integer lowest = *std::min_element(outputs.begin(), outputs.end());
        const integer highest = *std::max_element(outputs.begin(), outputs.end());
        const bool zero_qualifies = largest ? 0 <= lowest : highest <= 0;
        const integer base = empty_output.has_value() ? empty_output.value()
                             : zero_qualifies          ? integer(0)
                             : largest                 ? lowest
                                                       : highest;
        std::vector<poly_sample> samples;
        samples.reserve(pinned.size());
        for (const auto& [value, image] : pinned)
            samples.push_back(poly_sample{.inputs = {value}, .output = image});
        bodies_.clear();
        propose_element_hypotheses_.propose_hypotheses(convert_samples_.convert(samples), bodies_);
        for (const expr* body : bodies_) {
            // a constant g makes every output the same: the constant detector reads that
            if (find_literal_value_.find_value(body).has_value())
                continue;
            const expr* condition = largest ? make_functor_.make_functor(less_symbol_, {accumulator, body})
                                            : make_functor_.make_functor(less_symbol_, {body, accumulator});
            const expr* step = make_functor_.make_functor(
                piecewise_symbols_.piecewise,
                {make_functor_.make_functor(piecewise_symbols_.region, {condition, body}), accumulator});
            const expr* function =
                make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {step})});
            out.push_back(make_functor_.make_functor(
                symbols_.fold, {function, make_literal_.make_literal(base), input}));
        }
    }
}

#undef LIST_SELECTION_FOLD_HYPOTHESES
#undef LIST_SELECTION_FOLD_HYPOTHESES_TEMPLATE

#endif
