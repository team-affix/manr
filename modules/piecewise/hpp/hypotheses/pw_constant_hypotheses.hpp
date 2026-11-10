#ifndef PW_CONSTANT_HYPOTHESES_HPP
#define PW_CONSTANT_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_data_point.hpp"

// Domain-free hypothesis source: the extension term could be one output the data already shows. Only the best such
// constant is proposed: binding to `c` drops every branch whose output is `c` and leaves `c` as the catch-all, with
// nothing further to reduce, so the constant that saves the most nodes (branches dropped, less its own size)
// dominates every other one, and proposing the others would only dilute the search. Ties go to the first seen.
// Assumes outputs are already in their smallest form, as data is.
template<typename IGetCost>
struct pw_constant_hypotheses {
    pw_constant_hypotheses(IGetCost& get_cost);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IGetCost& get_cost_;
};

#define PW_CONSTANT_HYPOTHESES_TEMPLATE template<typename IGetCost>
#define PW_CONSTANT_HYPOTHESES          pw_constant_hypotheses<IGetCost>

PW_CONSTANT_HYPOTHESES_TEMPLATE PW_CONSTANT_HYPOTHESES::pw_constant_hypotheses(IGetCost& get_cost)
    : get_cost_(get_cost) {}

PW_CONSTANT_HYPOTHESES_TEMPLATE void PW_CONSTANT_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::vector<const expr*> outputs;
    std::unordered_map<const expr*, size_t> dropped;
    for (const pw_data_point& point : points) {
        const auto [entry, fresh] = dropped.emplace(point.output, 0);
        if (fresh)
            outputs.push_back(point.output);
        // the branch `case(input, output)`
        entry->second += 1 + get_cost_.cost(point.input) + get_cost_.cost(point.output);
    }
    const expr* best = nullptr;
    int64_t best_saving = 0;
    for (const expr* output : outputs) {
        const int64_t saving =
            static_cast<int64_t>(dropped.at(output)) - static_cast<int64_t>(get_cost_.cost(output));
        if (best != nullptr && saving <= best_saving)
            continue;
        best = output;
        best_saving = saving;
    }
    if (best == nullptr)
        return;
    out.push_back(best);
}

#undef PW_CONSTANT_HYPOTHESES
#undef PW_CONSTANT_HYPOTHESES_TEMPLATE

#endif
