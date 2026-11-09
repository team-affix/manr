#ifndef LIST_EXTREMUM_FOLD_HYPOTHESES_HPP
#define LIST_EXTREMUM_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

// A detector that reads list-to-integer data as a selection fold, the largest or the smallest of the elements and a
// base: `fold(pw(when(lt(acc, h), h), acc), b, xs)` for the largest, `fold(pw(when(lt(h, acc), h), acc), b, xs)` for
// the smallest. It reads whole lists and needs no tails. A base shows only where an output lies beyond every element
// of its list (the empty list's output always does), so each such output is a candidate; so is one base no further
// out than any list's extreme, which leaves every list to its elements (0 when it qualifies, else that extreme). The
// direction and base reproducing the most lists are proposed, when they reproduce more than half of them and at least
// two; the binder keeps the rest as branches, so a wrong label beyond its list's elements cannot become the base.
template<typename IFindElements, typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct list_extremum_fold_hypotheses {
    list_extremum_fold_hypotheses(IFindElements& find_elements,
                                  IFindLiteralValue& find_literal_value,
                                  IMakeLiteral& make_literal,
                                  IMakeFunctor& make_functor,
                                  list_symbols symbols,
                                  pw_symbols piecewise_symbols,
                                  uint32_t less_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    pw_symbols piecewise_symbols_;
    uint32_t less_symbol_;
};

#define LIST_EXTREMUM_FOLD_HYPOTHESES_TEMPLATE \
    template<typename IFindElements, typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define LIST_EXTREMUM_FOLD_HYPOTHESES \
    list_extremum_fold_hypotheses<IFindElements, IFindLiteralValue, IMakeLiteral, IMakeFunctor>

LIST_EXTREMUM_FOLD_HYPOTHESES_TEMPLATE LIST_EXTREMUM_FOLD_HYPOTHESES::list_extremum_fold_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    pw_symbols piecewise_symbols,
    uint32_t less_symbol)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , piecewise_symbols_(piecewise_symbols)
    , less_symbol_(less_symbol) {}

LIST_EXTREMUM_FOLD_HYPOTHESES_TEMPLATE void LIST_EXTREMUM_FOLD_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<std::vector<integer>> lists;
    std::vector<integer> outputs;
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
        lists.push_back(std::move(list));
        outputs.push_back(output.value());
    }
    std::optional<bool> best_largest;
    integer best_base = 0;
    size_t best_reproduced = std::max<size_t>(lists.size() / 2, 1);
    for (const bool largest : {true, false}) {
        // base candidates: every output beyond every element of its list, and one no further out than any list's
        // extreme (0 when it qualifies)
        std::set<integer> bases;
        std::optional<integer> bound;
        for (size_t index = 0; index < lists.size(); ++index) {
            const std::vector<integer>& list = lists[index];
            if (list.empty()) {
                bases.insert(outputs[index]);
                continue;
            }
            const integer extreme = largest ? *std::max_element(list.begin(), list.end())
                                            : *std::min_element(list.begin(), list.end());
            const bool tighter = !bound.has_value() || (largest ? extreme < bound.value() : bound.value() < extreme);
            if (tighter)
                bound = extreme;
            const bool passes = largest ? extreme < outputs[index] : outputs[index] < extreme;
            if (passes)
                bases.insert(outputs[index]);
        }
        const bool zero_qualifies = !bound.has_value() || (largest ? 0 <= bound.value() : bound.value() <= 0);
        bases.insert(zero_qualifies ? integer(0) : bound.value());
        for (const integer& base : bases) {
            size_t reproduced = 0;
            for (size_t index = 0; index < lists.size(); ++index) {
                integer value = base;
                for (const integer& element : lists[index])
                    value = largest ? std::max(value, element) : std::min(value, element);
                if (value == outputs[index])
                    ++reproduced;
            }
            if (reproduced <= best_reproduced)
                continue;
            best_largest = largest;
            best_base = base;
            best_reproduced = reproduced;
        }
    }
    if (!best_largest.has_value())
        return;
    const expr* head = make_functor_.make_functor(symbols_.head, {});
    const expr* accumulator = make_functor_.make_functor(symbols_.accumulator, {});
    const expr* condition = best_largest.value() ? make_functor_.make_functor(less_symbol_, {accumulator, head})
                                                 : make_functor_.make_functor(less_symbol_, {head, accumulator});
    const expr* step = make_functor_.make_functor(
        piecewise_symbols_.piecewise, {make_functor_.make_functor(piecewise_symbols_.region, {condition, head}), accumulator});
    out.push_back(make_functor_.make_functor(
        symbols_.fold, {make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {step})}), make_literal_.make_literal(best_base), make_functor_.make_functor(symbols_.input, {})}));
}

#undef LIST_EXTREMUM_FOLD_HYPOTHESES
#undef LIST_EXTREMUM_FOLD_HYPOTHESES_TEMPLATE

#endif
