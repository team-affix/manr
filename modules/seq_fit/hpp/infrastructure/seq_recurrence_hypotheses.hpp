#ifndef SEQ_RECURRENCE_HYPOTHESES_HPP
#define SEQ_RECURRENCE_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/poly_sample.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/seq_symbols.hpp"

// A detector that reads integer sequence data (`point(n) -> a(n)`) as a recurrence of one order k,
// `rec(lam(...lam(s)), a(0), ..., a(k-1), n)`. Consecutive terms make every step visible: each index j >= k whose k
// previous terms are in the data gives the step point `(j, a(j-1), ..., a(j-k)) -> a(j)`, handed to the step domain
// (polynomial fitting over `#0`, `#1`, ..., `#k`), the same split as folds with tails. Each step body it proposes,
// closed under k + 1 lambdas, becomes a recurrence with the data's first k terms as bases. Needs those terms, and more step points than a step of degree
// one has unknowns; a constant sequence is left to the constant detector.
template<typename IFindLiteralValue, typename IConvertSamples, typename IProposeStepHypotheses, typename IMakeLiteral,
         typename IMakeFunctor>
struct seq_recurrence_hypotheses {
    seq_recurrence_hypotheses(IFindLiteralValue& find_literal_value,
                              IConvertSamples& convert_samples,
                              IProposeStepHypotheses& propose_step_hypotheses,
                              IMakeLiteral& make_literal,
                              IMakeFunctor& make_functor,
                              seq_symbols symbols,
                              uint32_t variable,
                              size_t order);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IFindLiteralValue& find_literal_value_;
    IConvertSamples& convert_samples_;
    IProposeStepHypotheses& propose_step_hypotheses_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    seq_symbols symbols_;
    uint32_t variable_;
    size_t order_;
    std::vector<const expr*> steps_;
};

#define SEQ_RECURRENCE_HYPOTHESES_TEMPLATE                                                                   \
    template<typename IFindLiteralValue, typename IConvertSamples, typename IProposeStepHypotheses, \
             typename IMakeLiteral, typename IMakeFunctor>
#define SEQ_RECURRENCE_HYPOTHESES                                                                         \
    seq_recurrence_hypotheses<IFindLiteralValue, IConvertSamples, IProposeStepHypotheses, IMakeLiteral, \
                              IMakeFunctor>

SEQ_RECURRENCE_HYPOTHESES_TEMPLATE SEQ_RECURRENCE_HYPOTHESES::seq_recurrence_hypotheses(
    IFindLiteralValue& find_literal_value,
    IConvertSamples& convert_samples,
    IProposeStepHypotheses& propose_step_hypotheses,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    seq_symbols symbols,
    uint32_t variable,
    size_t order)
    : find_literal_value_(find_literal_value)
    , convert_samples_(convert_samples)
    , propose_step_hypotheses_(propose_step_hypotheses)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , variable_(variable)
    , order_(order)
    , steps_() {}

SEQ_RECURRENCE_HYPOTHESES_TEMPLATE void SEQ_RECURRENCE_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::map<integer, integer> terms;
    for (const pw_data_point& point : points) {
        const expr::functor* input = std::get_if<expr::functor>(&point.input->content);
        if (input == nullptr || input->args.size() != 1)
            return;
        const std::optional<integer> index = find_literal_value_.find_value(input->args[0]);
        const std::optional<integer> term = find_literal_value_.find_value(point.output);
        if (!index.has_value() || !term.has_value())
            continue;
        terms.emplace(index.value(), term.value());
    }
    // a step of degree one in i and k previous terms has k + 2 unknowns; one more point checks it
    if (terms.size() < 2 * order_ + 3)
        return;
    bool constant = true;
    for (const auto& [index, term] : terms)
        constant = constant && term == terms.begin()->second;
    if (constant)
        return;
    std::vector<const expr*> arguments{nullptr};
    for (size_t position = 0; position < order_; ++position) {
        const auto base = terms.find(integer(position));
        if (base == terms.end())
            return;
        arguments.push_back(make_literal_.make_literal(base->second));
    }
    std::vector<poly_sample> samples;
    for (const auto& [index, term] : terms) {
        if (index < order_)
            continue;
        poly_sample sample{.inputs = {index}, .output = term};
        for (size_t back = 1; back <= order_; ++back) {
            const auto previous = terms.find(index - back);
            if (previous == terms.end())
                break;
            sample.inputs.push_back(previous->second);
        }
        if (sample.inputs.size() != order_ + 1)
            continue;
        samples.push_back(sample);
    }
    if (samples.size() < order_ + 3)
        return;
    steps_.clear();
    propose_step_hypotheses_.propose_hypotheses(convert_samples_.convert(samples), steps_);
    arguments.push_back(make_functor_.make_functor(variable_, {}));
    for (const expr* step : steps_) {
        arguments[0] = step;
        for (size_t binder = 0; binder <= order_; ++binder)
            arguments[0] = make_functor_.make_functor(symbols_.lambda, {arguments[0]});
        out.push_back(make_functor_.make_functor(symbols_.recurrence, arguments));
    }
}

#undef SEQ_RECURRENCE_HYPOTHESES
#undef SEQ_RECURRENCE_HYPOTHESES_TEMPLATE

#endif
