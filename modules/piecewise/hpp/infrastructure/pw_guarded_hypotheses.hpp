#ifndef PW_GUARDED_HYPOTHESES_HPP
#define PW_GUARDED_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

constexpr size_t k_pw_default_max_guards = 2;
constexpr size_t k_pw_default_max_models = 8;
constexpr size_t k_pw_default_min_guarded = 3;
constexpr size_t k_pw_default_max_guarded_proposals = 4;

// A detector that reads data as a guarded value, `pw(when(c, v), d)`: a default output d, and where the output is
// something else, a condition c and a value v, split outer-inner (`docs/decomposing-detection.md`). Assuming d, every
// point says whether c holds (its output is not d), which is condition data (`input -> 1` or `-> 0`) for the
// condition domain, and the points where it holds are data for the value domain. So the inner function of a sum that
// adds only the primes reads as `pw(when(is_prime(#0), #0), 0)`, and a count over any condition as
// `pw(when(c, 1), 0)`. Each of the `max_guards` most common outputs, when at least `min_guarded` points have it, is
// tried as d.
//
// The default may also be a model: each of the first `max_models` models the value domain proposes for all the
// points is evaluated at every point, and when it reproduces at least half of them but not all, it is d and the points
// it misses are the guarded ones. So data that follows `#0^2` except where a condition holds reads as
// `pw(when(c, v), #0^2)`: a piecewise whose branches are guarded by any condition the condition domain reads. Each
// condition with each value is checked against every point, and only exact readings are kept. A condition is read only from at least
// `min_guarded` guarded points and as many others: fewer exceptions are left as cases, since a condition read from one
// or two points does not generalize. Nothing when every output is the same, an input has two outputs, or one of the
// models already reproduces every point. Only the `max_proposals` smallest
// readings are proposed, since each is checked against all the data. The value domain's own proposals for all the
// points (the models tried as defaults) are passed through as well, so this stands in for the value domain where both
// are wanted, and fits the points once. Proposals are memoized per data set, since readings that try several starts
// ask again for the same data.
template<typename IProposeConditions, typename IProposeValues, typename IEvaluateAt, typename IFindCost,
         typename IMakeLiteral, typename IMakeFunctor>
struct pw_guarded_hypotheses {
    pw_guarded_hypotheses(IProposeConditions& propose_conditions,
                          IProposeValues& propose_values,
                          IEvaluateAt& evaluate_at,
                          IFindCost& find_cost,
                          IMakeLiteral& make_literal,
                          IMakeFunctor& make_functor,
                          pw_symbols symbols,
                          size_t max_guards = k_pw_default_max_guards,
                          size_t max_models = k_pw_default_max_models,
                          size_t min_guarded = k_pw_default_min_guarded,
                          size_t max_proposals = k_pw_default_max_guarded_proposals);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    void propose_fresh(const std::vector<pw_data_point>& points, const std::vector<const expr*>& models,
                       std::vector<const expr*>& out);
    void propose_guarded(const std::vector<pw_data_point>& points, const expr* fallback,
                         const std::vector<bool>& guarded, std::vector<const expr*>& out);

    IProposeConditions& propose_conditions_;
    IProposeValues& propose_values_;
    IEvaluateAt& evaluate_at_;
    IFindCost& find_cost_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    pw_symbols symbols_;
    size_t max_guards_;
    size_t max_models_;
    size_t min_guarded_;
    size_t max_proposals_;
    std::map<std::vector<pw_data_point>, std::vector<const expr*>> proposed_;
};

#define PW_GUARDED_HYPOTHESES_TEMPLATE                                                                      \
    template<typename IProposeConditions, typename IProposeValues, typename IEvaluateAt, typename IFindCost, \
             typename IMakeLiteral, typename IMakeFunctor>
#define PW_GUARDED_HYPOTHESES \
    pw_guarded_hypotheses<IProposeConditions, IProposeValues, IEvaluateAt, IFindCost, IMakeLiteral, IMakeFunctor>

PW_GUARDED_HYPOTHESES_TEMPLATE PW_GUARDED_HYPOTHESES::pw_guarded_hypotheses(IProposeConditions& propose_conditions,
                                                                            IProposeValues& propose_values,
                                                                            IEvaluateAt& evaluate_at,
                                                                            IFindCost& find_cost,
                                                                            IMakeLiteral& make_literal,
                                                                            IMakeFunctor& make_functor,
                                                                            pw_symbols symbols,
                                                                            size_t max_guards,
                                                                            size_t max_models,
                                                                            size_t min_guarded,
                                                                            size_t max_proposals)
    : propose_conditions_(propose_conditions)
    , propose_values_(propose_values)
    , evaluate_at_(evaluate_at)
    , find_cost_(find_cost)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_guards_(max_guards)
    , max_models_(max_models)
    , min_guarded_(min_guarded)
    , max_proposals_(max_proposals)
    , proposed_() {}

PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_hypotheses(const std::vector<pw_data_point>& points,
                                                                             std::vector<const expr*>& out) {
    auto known = proposed_.find(points);
    if (known == proposed_.end()) {
        std::vector<const expr*> models;
        propose_values_.propose_hypotheses(points, models);
        std::vector<const expr*> fresh;
        propose_fresh(points, models, fresh);
        std::vector<std::pair<size_t, const expr*>> ranked;
        for (const expr* reading : fresh)
            ranked.emplace_back(find_cost_.cost(reading), reading);
        std::sort(ranked.begin(), ranked.end());
        std::vector<const expr*> smallest = models;
        const size_t limit = models.size() + max_proposals_;
        for (const auto& [cost, reading] : ranked) {
            const bool fresh_reading = std::find(smallest.begin(), smallest.end(), reading) == smallest.end();
            if (smallest.size() < limit && fresh_reading)
                smallest.push_back(reading);
        }
        known = proposed_.emplace(points, smallest).first;
    }
    out.insert(out.end(), known->second.begin(), known->second.end());
}

PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_fresh(const std::vector<pw_data_point>& points,
                                                                        const std::vector<const expr*>& models,
                                                                        std::vector<const expr*>& out) {
    // data that gives one input two outputs is no function: nothing guards it
    std::map<const expr*, const expr*> outputs;
    for (const pw_data_point& point : points) {
        const auto [known, inserted] = outputs.emplace(point.input, point.output);
        if (!inserted && known->second != point.output)
            return;
    }
    std::map<const expr*, size_t> counts;
    for (const pw_data_point& point : points)
        ++counts[point.output];
    if (counts.size() < 2)
        return;
    // which points each model reproduces; a model that reproduces them all leaves nothing to guard
    std::vector<std::vector<bool>> misses;
    const size_t kept_models = std::min(models.size(), max_models_);
    for (size_t index = 0; index < kept_models; ++index) {
        std::vector<bool> missed;
        for (const pw_data_point& point : points) {
            const std::optional<const expr*> value = evaluate_at_.evaluate_at(models[index], point.input);
            missed.push_back(!value.has_value() || value.value() != point.output);
        }
        if (std::find(missed.begin(), missed.end(), true) == missed.end())
            return;
        misses.push_back(missed);
    }
    std::vector<std::pair<size_t, const expr*>> common;
    for (const auto& [output, count] : counts)
        common.emplace_back(count, output);
    std::sort(common.rbegin(), common.rend());
    // the most common outputs as defaults, when they are not singular (a value at most a few points share is no
    // default)
    for (size_t guard = 0; guard < common.size() && guard < max_guards_; ++guard) {
        if (common[guard].first < min_guarded_)
            break;
        std::vector<bool> guarded;
        for (const pw_data_point& point : points)
            guarded.push_back(point.output != common[guard].second);
        propose_guarded(points, common[guard].second, guarded, out);
    }
    // models as defaults: those that reproduce at least half of the points
    for (size_t index = 0; index < kept_models; ++index) {
        const size_t missed = static_cast<size_t>(std::count(misses[index].begin(), misses[index].end(), true));
        if (2 * missed > points.size())
            continue;
        propose_guarded(points, models[index], misses[index], out);
    }
}

// pw(when(c, v), fallback) for each condition read from the guarded points and each value fitted to them.
PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_guarded(const std::vector<pw_data_point>& points,
                                                                          const expr* fallback,
                                                                          const std::vector<bool>& guarded,
                                                                          std::vector<const expr*>& out) {
    const size_t count = static_cast<size_t>(std::count(guarded.begin(), guarded.end(), true));
    const bool readable = count >= min_guarded_ && points.size() - count >= min_guarded_;
    if (!readable)
        return;
    const expr* holds = make_literal_.make_literal(integer(1));
    const expr* fails = make_literal_.make_literal(integer(0));
    std::vector<pw_data_point> conditions_data;
    std::vector<pw_data_point> values_data;
    for (size_t index = 0; index < points.size(); ++index) {
        const expr* label = guarded[index] ? holds : fails;
        conditions_data.push_back(pw_data_point{.input = points[index].input, .output = label});
        if (guarded[index])
            values_data.push_back(points[index]);
    }
    std::vector<const expr*> conditions;
    propose_conditions_.propose_hypotheses(conditions_data, conditions);
    if (conditions.empty())
        return;
    std::vector<const expr*> values;
    propose_values_.propose_hypotheses(values_data, values);
    // only readings that reproduce every point: the domains may also propose near fits
    for (const expr* condition : conditions)
        for (const expr* value : values) {
            const expr* region = make_functor_.make_functor(symbols_.region, {condition, value});
            const expr* reading = make_functor_.make_functor(symbols_.piecewise, {region, fallback});
            bool exact = true;
            for (size_t index = 0; index < points.size() && exact; ++index) {
                const std::optional<const expr*> produced = evaluate_at_.evaluate_at(reading, points[index].input);
                exact = produced.has_value() && produced.value() == points[index].output;
            }
            if (exact)
                out.push_back(reading);
        }
}

#undef PW_GUARDED_HYPOTHESES
#undef PW_GUARDED_HYPOTHESES_TEMPLATE

#endif
