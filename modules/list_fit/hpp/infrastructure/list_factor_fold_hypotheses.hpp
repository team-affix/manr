#ifndef LIST_FACTOR_FOLD_HYPOTHESES_HPP
#define LIST_FACTOR_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_factor_fold.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads list-to-integer data as a product of an unknown inner function of the elements,
// `fold(lam(lam(g(#0) * #1)), b, xs)` (the head is `#0`, the accumulator `#1`), from whole lists: the outer-inner split with the outer combiner `*`
// (`docs/decomposing-detection.md`). `list_product_fold_hypotheses` is its specific case for linear `g`, kept beside
// it. The solver (`list_factor_fold_solver`) recovers `g` at each element value and the base; the result is checked
// exactly against every list, and proposed when it reproduces more than half of them. `g`'s values then go to the
// element domain, and each body it proposes, in `#0`, is `g` in the step as it stands, since the head is `#0` too. A `g` that is the identity, or the same factor
// at every value (`c^n`, an affine fold), is left to the specific detectors.
template<typename IFindElements,
         typename IFindLiteralValue,
         typename ISolveFold,
         typename IMultiplyFold,
         typename IConvertSamples,
         typename IProposeElementHypotheses,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct list_factor_fold_hypotheses {
    list_factor_fold_hypotheses(IFindElements& find_elements,
                                IFindLiteralValue& find_literal_value,
                                ISolveFold& solve_fold,
                                IMultiplyFold& multiply_fold,
                                IConvertSamples& convert_samples,
                                IProposeElementHypotheses& propose_element_hypotheses,
                                IMakeLiteral& make_literal,
                                IMakeFunctor& make_functor,
                                list_symbols symbols,
                                uint32_t multiply_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    ISolveFold& solve_fold_;
    IMultiplyFold& multiply_fold_;
    IConvertSamples& convert_samples_;
    IProposeElementHypotheses& propose_element_hypotheses_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    uint32_t multiply_symbol_;
    std::vector<const expr*> bodies_;
};

#define LIST_FACTOR_FOLD_HYPOTHESES_TEMPLATE                                                               \
    template<typename IFindElements, typename IFindLiteralValue, typename ISolveFold, typename IMultiplyFold, \
             typename IConvertSamples, typename IProposeElementHypotheses, typename IMakeLiteral, typename IMakeFunctor>
#define LIST_FACTOR_FOLD_HYPOTHESES                                                                    \
    list_factor_fold_hypotheses<IFindElements, IFindLiteralValue, ISolveFold, IMultiplyFold, IConvertSamples, \
                                IProposeElementHypotheses, IMakeLiteral, IMakeFunctor>

LIST_FACTOR_FOLD_HYPOTHESES_TEMPLATE LIST_FACTOR_FOLD_HYPOTHESES::list_factor_fold_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    ISolveFold& solve_fold,
    IMultiplyFold& multiply_fold,
    IConvertSamples& convert_samples,
    IProposeElementHypotheses& propose_element_hypotheses,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    uint32_t multiply_symbol)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , solve_fold_(solve_fold)
    , multiply_fold_(multiply_fold)
    , convert_samples_(convert_samples)
    , propose_element_hypotheses_(propose_element_hypotheses)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , multiply_symbol_(multiply_symbol)
    , bodies_() {}

LIST_FACTOR_FOLD_HYPOTHESES_TEMPLATE void LIST_FACTOR_FOLD_HYPOTHESES::propose_hypotheses(
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
    const size_t needed = std::max<size_t>(lists.size() / 2, 1);
    if (lists.size() <= needed)
        return;
    const std::optional<list_factor_fold> fold = solve_fold_.solve(lists, outputs);
    if (!fold.has_value())
        return;
    bool identity = true;
    bool constant = true;
    for (const auto& [value, factor] : fold.value().factors) {
        identity = identity && factor == value;
        constant = constant && factor == fold.value().factors.begin()->second;
    }
    if (identity || constant)
        return;
    size_t reproduced = 0;
    for (size_t index = 0; index < lists.size(); ++index)
        if (multiply_fold_.multiply(fold.value(), lists[index]) == outputs[index])
            ++reproduced;
    if (reproduced <= needed)
        return;
    std::vector<poly_sample> samples;
    samples.reserve(fold.value().factors.size());
    for (const auto& [value, factor] : fold.value().factors)
        samples.push_back(poly_sample{.inputs = {value}, .output = factor});
    bodies_.clear();
    propose_element_hypotheses_.propose_hypotheses(convert_samples_.convert(samples), bodies_);
    const expr* accumulator = make_functor_.make_functor(symbols_.accumulator, {});
    const expr* base = make_literal_.make_literal(fold.value().base);
    const expr* input = make_functor_.make_functor(symbols_.input, {});
    for (const expr* body : bodies_) {
        // a constant g makes every output the same: the constant detector reads that
        if (find_literal_value_.find_value(body).has_value())
            continue;
        const expr* step = make_functor_.make_functor(multiply_symbol_, {body, accumulator});
        const expr* function =
            make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {step})});
        out.push_back(make_functor_.make_functor(symbols_.fold, {function, base, input}));
    }
}

#undef LIST_FACTOR_FOLD_HYPOTHESES
#undef LIST_FACTOR_FOLD_HYPOTHESES_TEMPLATE

#endif
