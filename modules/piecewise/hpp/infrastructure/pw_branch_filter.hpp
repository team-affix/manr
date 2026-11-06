#ifndef PW_BRANCH_FILTER_HPP
#define PW_BRANCH_FILTER_HPP

#include <optional>
#include <vector>
#include "value_objects/expr.hpp"

// Keeps the branches an otherwise does not already reproduce. A branch `case(key, value)` is covered when the
// otherwise evaluates to `value` at `key`. An otherwise the domain cannot evaluate (e.g. one holding an extension
// term) covers nothing.
template<typename IEvaluateAt>
struct pw_branch_filter {
    pw_branch_filter(IEvaluateAt& evaluate_at);
    std::vector<const expr*> find_uncovered(const std::vector<const expr*>& branches, const expr* otherwise);
private:
    IEvaluateAt& evaluate_at_;
};

#define PW_BRANCH_FILTER_TEMPLATE template<typename IEvaluateAt>
#define PW_BRANCH_FILTER          pw_branch_filter<IEvaluateAt>

PW_BRANCH_FILTER_TEMPLATE PW_BRANCH_FILTER::pw_branch_filter(IEvaluateAt& evaluate_at)
    : evaluate_at_(evaluate_at) {}

PW_BRANCH_FILTER_TEMPLATE std::vector<const expr*> PW_BRANCH_FILTER::find_uncovered(
    const std::vector<const expr*>& branches, const expr* otherwise) {
    std::vector<const expr*> uncovered;
    for (const expr* branch : branches) {
        const expr::functor& f = std::get<expr::functor>(branch->content);
        const std::optional<const expr*> produced = evaluate_at_.evaluate_at(otherwise, f.args[0]);
        const bool covered = produced.has_value() && produced.value() == f.args[1];
        if (covered)
            continue;
        uncovered.push_back(branch);
    }
    return uncovered;
}

#undef PW_BRANCH_FILTER
#undef PW_BRANCH_FILTER_TEMPLATE

#endif
