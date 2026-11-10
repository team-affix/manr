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

constexpr size_t k_seq_default_max_backward_steps = 64;

// A detector that reads integer sequence data (`point(n) -> a(n)`) as a recurrence of one order k,
// `rec(lam(...lam(s)), a(0), ..., a(k-1), n)`. Consecutive terms make every step visible: each index j >= k whose k
// previous terms are in the data gives the step point `(j, a(j-1), ..., a(j-k)) -> a(j)`, handed to the step domain
// (polynomial fitting over `#0`, `#1`, ..., `#k`), the same split as folds with tails. Each step body it proposes,
// closed under k + 1 lambdas, becomes a recurrence with bases a(0), ..., a(k-1): the data's own when it has them,
// else solved by running the step backwards from the first k observed terms, at most `max_backward_steps` indices.
// A step back solves `a(j) = s(j, a(j-1), ..., a(j-k))` for the oldest term, read as a line through its values at 0
// and 1 and then checked exactly; a step that is not invertible there gives no recurrence. Needs more step points than
// a step of degree one has unknowns; a constant sequence is left to the constant detector.
template<typename IFindLiteralValue, typename IConvertSamples, typename IProposeStepHypotheses, typename IMakeLiteral,
         typename IMakeFunctor, typename IEvaluateStepAt>
struct seq_recurrence_hypotheses {
    seq_recurrence_hypotheses(IFindLiteralValue& find_literal_value,
                              IConvertSamples& convert_samples,
                              IProposeStepHypotheses& propose_step_hypotheses,
                              IMakeLiteral& make_literal,
                              IMakeFunctor& make_functor,
                              IEvaluateStepAt& evaluate_step_at,
                              seq_symbols symbols,
                              uint32_t variable,
                              uint32_t point_symbol,
                              size_t order,
                              size_t max_backward_steps = k_seq_default_max_backward_steps);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    std::optional<std::vector<integer>> find_bases(const expr* step, const std::map<integer, integer>& terms);
    std::optional<integer> evaluate_step(const expr* step, const std::vector<integer>& coordinates);

    IFindLiteralValue& find_literal_value_;
    IConvertSamples& convert_samples_;
    IProposeStepHypotheses& propose_step_hypotheses_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    IEvaluateStepAt& evaluate_step_at_;
    seq_symbols symbols_;
    uint32_t variable_;
    uint32_t point_symbol_;
    size_t order_;
    size_t max_backward_steps_;
    std::vector<const expr*> steps_;
};

#define SEQ_RECURRENCE_HYPOTHESES_TEMPLATE                                                                   \
    template<typename IFindLiteralValue, typename IConvertSamples, typename IProposeStepHypotheses, \
             typename IMakeLiteral, typename IMakeFunctor, typename IEvaluateStepAt>
#define SEQ_RECURRENCE_HYPOTHESES                                                                         \
    seq_recurrence_hypotheses<IFindLiteralValue, IConvertSamples, IProposeStepHypotheses, IMakeLiteral, \
                              IMakeFunctor, IEvaluateStepAt>

SEQ_RECURRENCE_HYPOTHESES_TEMPLATE SEQ_RECURRENCE_HYPOTHESES::seq_recurrence_hypotheses(
    IFindLiteralValue& find_literal_value,
    IConvertSamples& convert_samples,
    IProposeStepHypotheses& propose_step_hypotheses,
    IMakeLiteral& make_literal,
    IMakeFunctor& make_functor,
    IEvaluateStepAt& evaluate_step_at,
    seq_symbols symbols,
    uint32_t variable,
    uint32_t point_symbol,
    size_t order,
    size_t max_backward_steps)
    : find_literal_value_(find_literal_value)
    , convert_samples_(convert_samples)
    , propose_step_hypotheses_(propose_step_hypotheses)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , evaluate_step_at_(evaluate_step_at)
    , symbols_(symbols)
    , variable_(variable)
    , point_symbol_(point_symbol)
    , order_(order)
    , max_backward_steps_(max_backward_steps)
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
    for (const expr* step : steps_) {
        const std::optional<std::vector<integer>> bases = find_bases(step, terms);
        if (!bases.has_value())
            continue;
        std::vector<const expr*> arguments{step};
        for (size_t binder = 0; binder <= order_; ++binder)
            arguments[0] = make_functor_.make_functor(symbols_.lambda, {arguments[0]});
        for (const integer& base : bases.value())
            arguments.push_back(make_literal_.make_literal(base));
        arguments.push_back(make_functor_.make_functor(variable_, {}));
        out.push_back(make_functor_.make_functor(symbols_.recurrence, arguments));
    }
}

// a(0), ..., a(k-1): observed, or solved backwards from the k terms at the first observed index, one index at a time.
SEQ_RECURRENCE_HYPOTHESES_TEMPLATE std::optional<std::vector<integer>> SEQ_RECURRENCE_HYPOTHESES::find_bases(
    const expr* step, const std::map<integer, integer>& terms) {
    const integer first = terms.begin()->first;
    if (first < 0 || first > max_backward_steps_)
        return std::nullopt;
    // window: a(low), ..., a(low + k - 1)
    std::vector<integer> window;
    for (size_t position = 0; position < order_; ++position) {
        const auto term = terms.find(first + position);
        if (term == terms.end())
            return std::nullopt;
        window.push_back(term->second);
    }
    std::vector<integer> coordinates(order_ + 1);
    for (integer low = first; low > 0; --low) {
        // a(j) = s(j, a(j-1), ..., a(j-k+1), x) at j = low + k - 1, solved for x = a(low - 1)
        coordinates[0] = low + order_ - 1;
        for (size_t back = 1; back < order_; ++back)
            coordinates[back] = window[order_ - 1 - back];
        coordinates[order_] = 0;
        const std::optional<integer> at_zero = evaluate_step(step, coordinates);
        coordinates[order_] = 1;
        const std::optional<integer> at_one = evaluate_step(step, coordinates);
        if (!at_zero.has_value() || !at_one.has_value())
            return std::nullopt;
        const integer slope = at_one.value() - at_zero.value();
        const integer difference = window[order_ - 1] - at_zero.value();
        const bool invertible = slope != 0 && difference % slope == 0;
        if (!invertible)
            return std::nullopt;
        coordinates[order_] = difference / slope;
        const std::optional<integer> check = evaluate_step(step, coordinates);
        if (!check.has_value() || check.value() != window[order_ - 1])
            return std::nullopt;
        window.insert(window.begin(), coordinates[order_]);
        window.pop_back();
    }
    return window;
}

SEQ_RECURRENCE_HYPOTHESES_TEMPLATE std::optional<integer> SEQ_RECURRENCE_HYPOTHESES::evaluate_step(
    const expr* step, const std::vector<integer>& coordinates) {
    std::vector<const expr*> literals;
    literals.reserve(coordinates.size());
    for (const integer& coordinate : coordinates)
        literals.push_back(make_literal_.make_literal(coordinate));
    const std::optional<const expr*> value =
        evaluate_step_at_.evaluate_at(step, make_functor_.make_functor(point_symbol_, literals));
    if (!value.has_value())
        return std::nullopt;
    return find_literal_value_.find_value(value.value());
}

#undef SEQ_RECURRENCE_HYPOTHESES
#undef SEQ_RECURRENCE_HYPOTHESES_TEMPLATE

#endif
