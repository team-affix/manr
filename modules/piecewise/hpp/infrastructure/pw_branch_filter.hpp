#ifndef PW_BRANCH_FILTER_HPP
#define PW_BRANCH_FILTER_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_symbols.hpp"

// Keeps the branches an otherwise does not already reproduce. A branch `case(key, value)` is covered when the
// otherwise's value at `key` matches `value`: equal, or equal under a binding of the holes `value` holds (an unknown
// label `?1`, or `2 * ?2`), with one binding shared by every branch of the call. An otherwise the domain cannot
// evaluate (e.g. one holding an extension term) covers nothing. A `when` branch is never covered: it is not a data
// point. Dropping a covered branch drops its holes, so this assumes they occur nowhere outside the piecewise.
template<typename IEvaluateAt, typename IMatchOutput>
struct pw_branch_filter {
    pw_branch_filter(IEvaluateAt& evaluate_at, IMatchOutput& match_output, pw_symbols symbols);
    std::vector<const expr*> find_uncovered(const std::vector<const expr*>& branches, const expr* otherwise);
private:
    IEvaluateAt& evaluate_at_;
    IMatchOutput& match_output_;
    pw_symbols symbols_;
};

#define PW_BRANCH_FILTER_TEMPLATE template<typename IEvaluateAt, typename IMatchOutput>
#define PW_BRANCH_FILTER          pw_branch_filter<IEvaluateAt, IMatchOutput>

PW_BRANCH_FILTER_TEMPLATE PW_BRANCH_FILTER::pw_branch_filter(
    IEvaluateAt& evaluate_at, IMatchOutput& match_output, pw_symbols symbols)
    : evaluate_at_(evaluate_at)
    , match_output_(match_output)
    , symbols_(symbols) {}

PW_BRANCH_FILTER_TEMPLATE std::vector<const expr*> PW_BRANCH_FILTER::find_uncovered(
    const std::vector<const expr*>& branches, const expr* otherwise) {
    std::vector<const expr*> uncovered;
    std::map<uint32_t, const expr*> bindings;
    for (const expr* branch : branches) {
        const expr::functor& f = std::get<expr::functor>(branch->content);
        if (f.id != symbols_.branch) {
            uncovered.push_back(branch);
            continue;
        }
        const std::optional<const expr*> produced = evaluate_at_.evaluate_at(otherwise, f.args[0]);
        const bool covered = produced.has_value() && match_output_.match(f.args[1], produced.value(), bindings);
        if (covered)
            continue;
        uncovered.push_back(branch);
    }
    return uncovered;
}

#undef PW_BRANCH_FILTER
#undef PW_BRANCH_FILTER_TEMPLATE

#endif
