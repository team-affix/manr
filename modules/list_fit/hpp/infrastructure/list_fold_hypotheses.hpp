#ifndef LIST_FOLD_HYPOTHESES_HPP
#define LIST_FOLD_HYPOTHESES_HPP

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector that reads data over lists as a right fold, `fold(s, b, xs)`, for elements and results of any domain.
// The base `b` is the output for the empty list. Every point whose input is `cons(h, t)`, where `t` is itself an
// input of the data, gives one step data point `point(h, output of t) -> output`. The step data points are handed to
// the step domain (variables `h`, `acc`), and each step it proposes becomes `fold(s, b, xs)`. Needs the empty list
// and tails in the data; without them it proposes nothing. The first output seen for a step input is kept; step data
// points keep the order of the data.
template<typename IProposeStepHypotheses, typename IMakeFunctor>
struct list_fold_hypotheses {
    list_fold_hypotheses(IProposeStepHypotheses& propose_step_hypotheses,
                         IMakeFunctor& make_functor,
                         list_symbols symbols,
                         uint32_t point_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IProposeStepHypotheses& propose_step_hypotheses_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    uint32_t point_symbol_;
    std::vector<const expr*> steps_;
};

#define LIST_FOLD_HYPOTHESES_TEMPLATE template<typename IProposeStepHypotheses, typename IMakeFunctor>
#define LIST_FOLD_HYPOTHESES          list_fold_hypotheses<IProposeStepHypotheses, IMakeFunctor>

LIST_FOLD_HYPOTHESES_TEMPLATE LIST_FOLD_HYPOTHESES::list_fold_hypotheses(
    IProposeStepHypotheses& propose_step_hypotheses, IMakeFunctor& make_functor, list_symbols symbols,
    uint32_t point_symbol)
    : propose_step_hypotheses_(propose_step_hypotheses)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , point_symbol_(point_symbol)
    , steps_() {}

LIST_FOLD_HYPOTHESES_TEMPLATE void LIST_FOLD_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::unordered_map<const expr*, const expr*> outputs;
    for (const pw_data_point& point : points)
        outputs.emplace(point.input, point.output);
    const auto base = outputs.find(make_functor_.make_functor(symbols_.nil, {}));
    if (base == outputs.end())
        return;
    std::vector<pw_data_point> steps;
    std::unordered_set<const expr*> seen;
    for (const pw_data_point& point : points) {
        const expr::functor* f = std::get_if<expr::functor>(&point.input->content);
        if (f == nullptr || f->id != symbols_.cons || f->args.size() != 2)
            continue;
        const auto tail = outputs.find(f->args[1]);
        if (tail == outputs.end())
            continue;
        const expr* input = make_functor_.make_functor(point_symbol_, {f->args[0], tail->second});
        const bool fresh = seen.insert(input).second;
        if (!fresh)
            continue;
        steps.push_back(pw_data_point{.input = input, .output = point.output});
    }
    if (steps.empty())
        return;
    steps_.clear();
    propose_step_hypotheses_.propose_hypotheses(steps, steps_);
    const expr* list = make_functor_.make_functor(symbols_.input, {});
    for (const expr* step : steps_) {
        const expr* function =
            make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {step})});
        out.push_back(make_functor_.make_functor(symbols_.fold, {function, base->second, list}));
    }
}

#undef LIST_FOLD_HYPOTHESES
#undef LIST_FOLD_HYPOTHESES_TEMPLATE

#endif
