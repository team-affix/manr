#ifndef LIST_LISTED_FOLD_HYPOTHESES_HPP
#define LIST_LISTED_FOLD_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/list_symbols.hpp"
#include "value_objects/pw_data_point.hpp"

constexpr size_t k_list_default_max_listed_step = 7;

// A detector that reads list-to-list data as a fold whose accumulator is a list, `fold(lam(lam(s)), b, #0)` with s
// built from `cons`, `append`, `nil`, the head `#0` and the accumulator `#1`: reverse is
// `append(#1, cons(#0, nil))`, duplicating each element `cons(#0, cons(#0, #1))`. Such steps are few and small, so this
// is a specific detector by enumeration: every list-valued step up to `max_size` nodes is run over the data directly
// (on the lists' elements, without interning the intermediate lists), and a step that reproduces more than half of
// the lists is proposed. The base is the empty list's output, else `nil`. Lengths refute first: a step's result has
// `a * length(acc) + b` elements (a its `#1`s, b its `cons`es), so a list's final length is predicted in one pass,
// stopping once it passes the output's; only a list whose length matches is run. This keeps steps that double the
// accumulator (`append(#1, #1)`) from building lists of 2^n elements. A step is refuted as soon as it misses too many
// lists to reproduce more than half, so most steps stop after a few lists.
// The copy (`cons(#0, #1)`, the input itself) and steps ignoring the head are left out.
template<typename IFindElements, typename IMakeFunctor>
struct list_listed_fold_hypotheses {
    list_listed_fold_hypotheses(IFindElements& find_elements,
                                IMakeFunctor& make_functor,
                                list_symbols symbols,
                                size_t max_size = k_list_default_max_listed_step);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    void enumerate_steps();
    void run_step(const expr* step, const expr* head, std::vector<const expr*>& accumulator) const;
    size_t count_nodes(const expr* step, uint32_t symbol) const;

    IFindElements& find_elements_;
    IMakeFunctor& make_functor_;
    list_symbols symbols_;
    size_t max_size_;
    std::vector<const expr*> steps_;
    std::vector<size_t> accumulator_counts_;
    std::vector<size_t> cons_counts_;
};

#define LIST_LISTED_FOLD_HYPOTHESES_TEMPLATE template<typename IFindElements, typename IMakeFunctor>
#define LIST_LISTED_FOLD_HYPOTHESES          list_listed_fold_hypotheses<IFindElements, IMakeFunctor>

LIST_LISTED_FOLD_HYPOTHESES_TEMPLATE LIST_LISTED_FOLD_HYPOTHESES::list_listed_fold_hypotheses(
    IFindElements& find_elements, IMakeFunctor& make_functor, list_symbols symbols, size_t max_size)
    : find_elements_(find_elements)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_size_(max_size)
    , steps_()
    , accumulator_counts_()
    , cons_counts_() {}

LIST_LISTED_FOLD_HYPOTHESES_TEMPLATE void LIST_LISTED_FOLD_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<std::vector<const expr*>> inputs;
    std::vector<std::vector<const expr*>> outputs;
    std::optional<const expr*> empty_output;
    for (const pw_data_point& point : points) {
        const std::optional<std::vector<const expr*>> input = find_elements_.find_elements(point.input);
        const std::optional<std::vector<const expr*>> output = find_elements_.find_elements(point.output);
        if (!input.has_value() || !output.has_value())
            continue;
        if (input.value().empty() && !empty_output.has_value())
            empty_output = point.output;
        inputs.push_back(input.value());
        outputs.push_back(output.value());
    }
    const size_t needed = std::max<size_t>(inputs.size() / 2, 1);
    if (inputs.size() <= needed)
        return;
    const expr* nil = make_functor_.make_functor(symbols_.nil, {});
    const expr* base = empty_output.has_value() ? empty_output.value() : nil;
    const std::optional<std::vector<const expr*>> base_elements = find_elements_.find_elements(base);
    if (!base_elements.has_value())
        return;
    if (steps_.empty())
        enumerate_steps();
    for (size_t candidate = 0; candidate < steps_.size(); ++candidate) {
        const expr* step = steps_[candidate];
        size_t reproduced = 0;
        size_t missed = 0;
        const size_t affordable = inputs.size() - needed - 1;
        for (size_t index = 0; index < inputs.size() && missed <= affordable; ++index) {
            // the predicted length, given up once it passes the output's
            size_t length = base_elements.value().size();
            for (size_t step_index = 0; step_index < inputs[index].size() && length <= outputs[index].size();
                 ++step_index)
                length = accumulator_counts_[candidate] * length + cons_counts_[candidate];
            bool matches = length == outputs[index].size();
            std::vector<const expr*> accumulator = base_elements.value();
            for (auto element = inputs[index].rbegin(); element != inputs[index].rend() && matches; ++element)
                run_step(step, *element, accumulator);
            matches = matches && accumulator == outputs[index];
            reproduced += matches ? 1 : 0;
            missed += matches ? 0 : 1;
        }
        if (reproduced <= needed)
            continue;
        const expr* function =
            make_functor_.make_functor(symbols_.lambda, {make_functor_.make_functor(symbols_.lambda, {step})});
        out.push_back(make_functor_.make_functor(
            symbols_.fold, {function, base, make_functor_.make_functor(symbols_.input, {})}));
    }
}

// Every list-valued step up to `max_size_` nodes, by size: `nil` and `#1`, then `cons(#0, l)` and `append(l, m)`. The
// copy and steps that never read the head (no `cons`) are dropped.
LIST_LISTED_FOLD_HYPOTHESES_TEMPLATE void LIST_LISTED_FOLD_HYPOTHESES::enumerate_steps() {
    const expr* head = make_functor_.make_functor(symbols_.head, {});
    std::vector<std::vector<const expr*>> by_size(max_size_ + 1);
    by_size[1] = {make_functor_.make_functor(symbols_.nil, {}), make_functor_.make_functor(symbols_.accumulator, {})};
    for (size_t size = 2; size <= max_size_; ++size) {
        if (size >= 3)
            for (const expr* tail : by_size[size - 2])
                by_size[size].push_back(make_functor_.make_functor(symbols_.cons, {head, tail}));
        for (size_t left = 1; left + 1 < size; ++left)
            for (const expr* first : by_size[left])
                for (const expr* second : by_size[size - 1 - left])
                    by_size[size].push_back(make_functor_.make_functor(symbols_.append, {first, second}));
    }
    const expr* copy =
        make_functor_.make_functor(symbols_.cons, {head, make_functor_.make_functor(symbols_.accumulator, {})});
    for (const std::vector<const expr*>& steps : by_size)
        for (const expr* step : steps) {
            const size_t cons_count = count_nodes(step, symbols_.cons);
            if (step == copy || cons_count == 0)
                continue;
            steps_.push_back(step);
            accumulator_counts_.push_back(count_nodes(step, symbols_.accumulator));
            cons_counts_.push_back(cons_count);
        }
}

// Runs the step on the head and the accumulator's elements, replacing them with the result.
LIST_LISTED_FOLD_HYPOTHESES_TEMPLATE void LIST_LISTED_FOLD_HYPOTHESES::run_step(
    const expr* step, const expr* head, std::vector<const expr*>& accumulator) const {
    const expr::functor& f = std::get<expr::functor>(step->content);
    if (f.id == symbols_.nil) {
        accumulator.clear();
        return;
    }
    if (f.id == symbols_.accumulator)
        return;
    if (f.id == symbols_.cons) {
        run_step(f.args[1], head, accumulator);
        accumulator.insert(accumulator.begin(), head);
        return;
    }
    std::vector<const expr*> second = accumulator;
    run_step(f.args[0], head, accumulator);
    run_step(f.args[1], head, second);
    accumulator.insert(accumulator.end(), second.begin(), second.end());
}

// The number of nodes of the step that are `symbol`.
LIST_LISTED_FOLD_HYPOTHESES_TEMPLATE size_t LIST_LISTED_FOLD_HYPOTHESES::count_nodes(
    const expr* step, uint32_t symbol) const {
    const expr::functor& f = std::get<expr::functor>(step->content);
    size_t count = f.id == symbol ? 1 : 0;
    for (const expr* arg : f.args)
        count += count_nodes(arg, symbol);
    return count;
}

#undef LIST_LISTED_FOLD_HYPOTHESES
#undef LIST_LISTED_FOLD_HYPOTHESES_TEMPLATE

#endif
