#ifndef LIST_COUNT_FOLD_HYPOTHESES_HPP
#define LIST_COUNT_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_monomial.hpp"
#include "value_objects/poly_polynomial.hpp"
#include "value_objects/poly_power.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

// A detector that reads list-to-integer data as a counting fold, a base plus the number of elements past a threshold:
// `fold(pw(when(lt(d, h), acc + 1), acc), b, xs)` counts the elements above d, `fold(pw(when(lt(h, d), acc + 1),
// acc), b, xs)` those below. It reads whole lists and needs no tails. Each element value of the data is tried as d in
// both directions (a threshold between two values counts the same as the value below or above it), with the lists
// sorted once so each count is a binary search; the base is a starting list's output less its count (the first list,
// then one halfway along, so a fit can begin past a wrong label). Counting every element
// is the length, left to the affine detector. The threshold and base reproducing the most lists are proposed, when
// they reproduce more than half of them and at least two; the binder keeps the rest as branches. A fold reproducing
// every list ends the search.
template<typename IFindElements, typename IFindLiteralValue, typename IRender, typename IMakeLiteral,
         typename IMakeFunctor>
struct list_count_fold_hypotheses {
    list_count_fold_hypotheses(IFindElements& find_elements,
                               IFindLiteralValue& find_literal_value,
                               IRender& render,
                               IMakeLiteral& make_literal,
                               IMakeFunctor& make_functor,
                               list_symbols symbols,
                               pw_symbols piecewise_symbols,
                               uint32_t less_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    integer count(const std::vector<integer>& sorted_list, const integer& threshold, bool above) const;

    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    IRender& render_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    pw_symbols piecewise_symbols_;
    uint32_t less_symbol_;
};

#define LIST_COUNT_FOLD_HYPOTHESES_TEMPLATE                                                        \
    template<typename IFindElements, typename IFindLiteralValue, typename IRender, typename IMakeLiteral, \
             typename IMakeFunctor>
#define LIST_COUNT_FOLD_HYPOTHESES \
    list_count_fold_hypotheses<IFindElements, IFindLiteralValue, IRender, IMakeLiteral, IMakeFunctor>

LIST_COUNT_FOLD_HYPOTHESES_TEMPLATE LIST_COUNT_FOLD_HYPOTHESES::list_count_fold_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    IRender& render,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    pw_symbols piecewise_symbols,
    uint32_t less_symbol)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , render_(render)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , piecewise_symbols_(piecewise_symbols)
    , less_symbol_(less_symbol) {}

LIST_COUNT_FOLD_HYPOTHESES_TEMPLATE void LIST_COUNT_FOLD_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<std::vector<integer>> lists;
    std::vector<integer> outputs;
    std::set<integer> values;
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
        std::sort(list.begin(), list.end());
        values.insert(list.begin(), list.end());
        lists.push_back(std::move(list));
        outputs.push_back(output.value());
    }
    std::optional<bool> best_above;
    integer best_threshold = 0;
    integer best_base = 0;
    size_t best_reproduced = std::max<size_t>(lists.size() / 2, 1);
    if (lists.size() <= best_reproduced)
        return;
    bool complete = false;
    for (const bool above : {true, false}) {
        for (const integer& threshold : values) {
            if (complete)
                break;
            // counting nothing or everything is a constant or the length, not a count past a threshold
            const bool splits = above ? threshold < *values.rbegin() : *values.begin() < threshold;
            if (!splits)
                continue;
            for (size_t start : {size_t(0), lists.size() / 2}) {
                // the base: the starting list's output less its count; a fold that cannot beat the best is
                // abandoned at its first failure past the slack
                const integer base = outputs[start] - count(lists[start], threshold, above);
                const size_t slack = lists.size() - best_reproduced - 1;
                size_t failures = 0;
                for (size_t index = 0; index < lists.size() && failures <= slack; ++index)
                    if (base + count(lists[index], threshold, above) != outputs[index])
                        ++failures;
                if (failures > slack)
                    continue;
                best_above = above;
                best_threshold = threshold;
                best_base = base;
                best_reproduced = lists.size() - failures;
                complete = failures == 0;
                if (complete)
                    break;
            }
        }
    }
    if (!best_above.has_value())
        return;
    const expr* head = make_functor_.make_functor(symbols_.head, {});
    const expr* accumulator = make_functor_.make_functor(symbols_.accumulator, {});
    const expr* threshold = make_literal_.make_literal(best_threshold);
    const expr* condition = best_above.value() ? make_functor_.make_functor(less_symbol_, {threshold, head})
                                               : make_functor_.make_functor(less_symbol_, {head, threshold});
    poly_polynomial incremented;
    incremented.terms.emplace(poly_monomial{.powers = {poly_power{.variable = symbols_.accumulator, .exponent = 1}}}, 1);
    incremented.terms.emplace(poly_monomial{}, 1);
    const expr* step = make_functor_.make_functor(
        piecewise_symbols_.piecewise,
        {make_functor_.make_functor(piecewise_symbols_.region, {condition, render_.render(incremented)}), accumulator});
    out.push_back(make_functor_.make_functor(
        symbols_.fold, {step, make_literal_.make_literal(best_base), make_functor_.make_functor(symbols_.input, {})}));
}

// The number of elements of the sorted list above the threshold, or below it.
LIST_COUNT_FOLD_HYPOTHESES_TEMPLATE integer LIST_COUNT_FOLD_HYPOTHESES::count(const std::vector<integer>& sorted_list,
                                                                             const integer& threshold,
                                                                             bool above) const {
    const auto boundary = above ? std::upper_bound(sorted_list.begin(), sorted_list.end(), threshold)
                                : std::lower_bound(sorted_list.begin(), sorted_list.end(), threshold);
    return above ? integer(sorted_list.end() - boundary) : integer(boundary - sorted_list.begin());
}

#undef LIST_COUNT_FOLD_HYPOTHESES
#undef LIST_COUNT_FOLD_HYPOTHESES_TEMPLATE

#endif
