#ifndef LIST_INTEGER_FOLD_HYPOTHESES_HPP
#define LIST_INTEGER_FOLD_HYPOTHESES_HPP

#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"

// The integer case of `list_fold_hypotheses`, kept beside it: step samples are integers, ordered by value, and handed
// to the step domain as integer samples. Reads list-to-integer data as a right fold, `fold(s, b, xs)`. The base `b`
// is the output for the empty list. Every point whose input is `cons(h, t)`, where `t` is itself an input of the
// data, gives one step sample `(h, output of t) -> output`. The step samples are handed to the step domain
// (variables `h`, `acc`), and each step it proposes becomes `fold(s, b, xs)`. Needs the empty list and tails in the
// data; without them it proposes nothing. The first output seen for a step input is kept.
template<typename IFindLiteralValue, typename IConvertSamples, typename IProposeStepHypotheses, typename IMakeFunctor>
struct list_integer_fold_hypotheses {
    list_integer_fold_hypotheses(IFindLiteralValue& find_literal_value,
                         IConvertSamples& convert_samples,
                         IProposeStepHypotheses& propose_step_hypotheses,
                         IMakeFunctor& make_functor,
                         list_symbols symbols);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindLiteralValue& find_literal_value_;
    IConvertSamples& convert_samples_;
    IProposeStepHypotheses& propose_step_hypotheses_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    std::vector<const expr*> steps_;
};

#define LIST_INTEGER_FOLD_HYPOTHESES_TEMPLATE                                                                    \
    template<typename IFindLiteralValue, typename IConvertSamples, typename IProposeStepHypotheses, \
             typename IMakeFunctor>
#define LIST_INTEGER_FOLD_HYPOTHESES \
    list_integer_fold_hypotheses<IFindLiteralValue, IConvertSamples, IProposeStepHypotheses, IMakeFunctor>

LIST_INTEGER_FOLD_HYPOTHESES_TEMPLATE LIST_INTEGER_FOLD_HYPOTHESES::list_integer_fold_hypotheses(
    IFindLiteralValue& find_literal_value,
    IConvertSamples& convert_samples,
    IProposeStepHypotheses& propose_step_hypotheses,
    IMakeFunctor& make_functor,
    list_symbols symbols)
    : find_literal_value_(find_literal_value)
    , convert_samples_(convert_samples)
    , propose_step_hypotheses_(propose_step_hypotheses)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , steps_() {}

LIST_INTEGER_FOLD_HYPOTHESES_TEMPLATE void LIST_INTEGER_FOLD_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    const expr* nil = make_functor_.make_functor(symbols_.nil, {});
    std::unordered_map<const expr*, const expr*> outputs;
    for (const pw_data_point& point : points) {
        if (!find_literal_value_.find_value(point.output).has_value())
            continue;
        outputs.emplace(point.input, point.output);
    }
    const auto base = outputs.find(nil);
    if (base == outputs.end())
        return;
    std::map<std::pair<integer, integer>, integer> steps;
    for (const pw_data_point& point : points) {
        const expr::functor* f = std::get_if<expr::functor>(&point.input->content);
        if (f == nullptr || f->id != symbols_.cons || f->args.size() != 2)
            continue;
        const auto tail = outputs.find(f->args[1]);
        if (tail == outputs.end())
            continue;
        const std::optional<integer> head = find_literal_value_.find_value(f->args[0]);
        const std::optional<integer> result = find_literal_value_.find_value(point.output);
        if (!head.has_value() || !result.has_value())
            continue;
        steps.emplace(std::pair{head.value(), find_literal_value_.find_value(tail->second).value()}, result.value());
    }
    if (steps.empty())
        return;
    std::vector<poly_sample> samples;
    samples.reserve(steps.size());
    for (const auto& [input, result] : steps)
        samples.push_back(poly_sample{.inputs = {input.first, input.second}, .output = result});
    steps_.clear();
    propose_step_hypotheses_.propose_hypotheses(convert_samples_.convert(samples), steps_);
    const expr* list = make_functor_.make_functor(symbols_.input, {});
    for (const expr* step : steps_)
        out.push_back(make_functor_.make_functor(symbols_.fold, {make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {step})}), base->second, list}));
}

#undef LIST_INTEGER_FOLD_HYPOTHESES
#undef LIST_INTEGER_FOLD_HYPOTHESES_TEMPLATE

#endif
