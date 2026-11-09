#ifndef LIST_SUM_FOLD_HYPOTHESES_HPP
#define LIST_SUM_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_sum_fold.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

constexpr size_t k_list_default_max_sum_starts = 4;

// A detector that reads list-to-integer data as a sum of an unknown inner function of the elements,
// `fold(lam(lam(g(#0) + #1)), b, xs)` (the head is `#0`, the accumulator `#1`), from whole lists: the outer-inner split
// with the outer combiner `+` (`docs/decomposing-detection.md`). A count on any condition is the case where g is 0 or
// 1. `list_affine_fold_hypotheses` covers polynomial g, kept beside it. The solver (`list_sum_fold_solver`) recovers g
// at each element value and the base by an exact solve, from a few starting lists so a wrong label among the first
// does not spoil it; the result is checked against every list, and proposed when it reproduces more than half of them.
// g's values then go to the element domain (whose residue detector reads a parity as a periodic piecewise), and each
// body it proposes, in `#0`, is g in the step as it stands. A g that is the identity or constant (a sum, a length) is
// left to the affine detector, and a constant body to the constant detector.
template<typename IFindElements,
         typename IFindLiteralValue,
         typename ISolveFold,
         typename ISumFold,
         typename IConvertSamples,
         typename IProposeElementHypotheses,
         typename IMakeLiteral,
         typename IMakeFunctor>
struct list_sum_fold_hypotheses {
    list_sum_fold_hypotheses(IFindElements& find_elements,
                                IFindLiteralValue& find_literal_value,
                                ISolveFold& solve_fold,
                                ISumFold& sum_fold,
                                IConvertSamples& convert_samples,
                                IProposeElementHypotheses& propose_element_hypotheses,
                                IMakeLiteral& make_literal,
                                IMakeFunctor& make_functor,
                                list_symbols symbols,
                                pw_symbols piecewise_symbols,
                                uint32_t add_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    const expr* add_accumulator(const expr* body, const expr* accumulator);

    IFindElements& find_elements_;
    IFindLiteralValue& find_literal_value_;
    ISolveFold& solve_fold_;
    ISumFold& sum_fold_;
    IConvertSamples& convert_samples_;
    IProposeElementHypotheses& propose_element_hypotheses_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    pw_symbols piecewise_symbols_;
    uint32_t add_symbol_;
    std::vector<const expr*> bodies_;
};

#define LIST_SUM_FOLD_HYPOTHESES_TEMPLATE                                                               \
    template<typename IFindElements, typename IFindLiteralValue, typename ISolveFold, typename ISumFold, \
             typename IConvertSamples, typename IProposeElementHypotheses, typename IMakeLiteral, typename IMakeFunctor>
#define LIST_SUM_FOLD_HYPOTHESES                                                                    \
    list_sum_fold_hypotheses<IFindElements, IFindLiteralValue, ISolveFold, ISumFold, IConvertSamples, \
                                IProposeElementHypotheses, IMakeLiteral, IMakeFunctor>

LIST_SUM_FOLD_HYPOTHESES_TEMPLATE LIST_SUM_FOLD_HYPOTHESES::list_sum_fold_hypotheses(
    IFindElements& find_elements,
    IFindLiteralValue& find_literal_value,
    ISolveFold& solve_fold,
    ISumFold& sum_fold,
    IConvertSamples& convert_samples,
    IProposeElementHypotheses& propose_element_hypotheses,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    list_symbols symbols,
    pw_symbols piecewise_symbols,
    uint32_t add_symbol)
    : find_elements_(find_elements)
    , find_literal_value_(find_literal_value)
    , solve_fold_(solve_fold)
    , sum_fold_(sum_fold)
    , convert_samples_(convert_samples)
    , propose_element_hypotheses_(propose_element_hypotheses)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , piecewise_symbols_(piecewise_symbols)
    , add_symbol_(add_symbol)
    , bodies_() {}

LIST_SUM_FOLD_HYPOTHESES_TEMPLATE void LIST_SUM_FOLD_HYPOTHESES::propose_hypotheses(
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
    // the first start, then a few spread over the lists, until one reproduces more than half of them
    std::optional<list_sum_fold> fold;
    size_t reproduced = 0;
    for (size_t attempt = 0; attempt < k_list_default_max_sum_starts && reproduced <= needed; ++attempt) {
        fold = solve_fold_.solve(lists, outputs, attempt * lists.size() / k_list_default_max_sum_starts);
        if (!fold.has_value())
            continue;
        reproduced = 0;
        for (size_t index = 0; index < lists.size(); ++index)
            if (sum_fold_.sum(fold.value(), lists[index]) == outputs[index])
                ++reproduced;
    }
    if (!fold.has_value() || reproduced <= needed)
        return;
    bool identity = true;
    bool constant = true;
    for (const auto& [value, term] : fold.value().terms) {
        identity = identity && term == value;
        constant = constant && term == fold.value().terms.begin()->second;
    }
    if (identity || constant)
        return;
    std::vector<poly_sample> samples;
    samples.reserve(fold.value().terms.size());
    for (const auto& [value, term] : fold.value().terms)
        samples.push_back(poly_sample{.inputs = {value}, .output = term});
    bodies_.clear();
    propose_element_hypotheses_.propose_hypotheses(convert_samples_.convert(samples), bodies_);
    const expr* accumulator = make_functor_.make_functor(symbols_.accumulator, {});
    const expr* base = make_literal_.make_literal(fold.value().base);
    const expr* input = make_functor_.make_functor(symbols_.input, {});
    for (const expr* body : bodies_) {
        // a constant g is a length, which the affine detector reads
        if (find_literal_value_.find_value(body).has_value())
            continue;
        const expr* step = add_accumulator(body, accumulator);
        const expr* function =
            make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {step})});
        out.push_back(make_functor_.make_functor(symbols_.fold, {function, base, input}));
    }
}

// `body + acc`, with the addition pushed into each value of a piecewise body (`pw(when(c, v), w) + acc` becomes
// `pw(when(c, v + acc), w + acc)`), since the step domain evaluates a piecewise only at the top of an expression.
LIST_SUM_FOLD_HYPOTHESES_TEMPLATE const expr* LIST_SUM_FOLD_HYPOTHESES::add_accumulator(
    const expr* body, const expr* accumulator) {
    const expr::functor* f = std::get_if<expr::functor>(&body->content);
    const bool piecewise = f != nullptr && f->id == piecewise_symbols_.piecewise;
    if (!piecewise)
        return make_functor_.make_functor(add_symbol_, {body, accumulator});
    std::vector<const expr*> branches;
    for (size_t index = 0; index + 1 < f->args.size(); ++index) {
        const expr::functor& branch = std::get<expr::functor>(f->args[index]->content);
        branches.push_back(make_functor_.make_functor(
            branch.id, {branch.args[0], add_accumulator(branch.args[1], accumulator)}));
    }
    branches.push_back(add_accumulator(f->args.back(), accumulator));
    return make_functor_.make_functor(piecewise_symbols_.piecewise, branches);
}

#undef LIST_SUM_FOLD_HYPOTHESES
#undef LIST_SUM_FOLD_HYPOTHESES_TEMPLATE

#endif
