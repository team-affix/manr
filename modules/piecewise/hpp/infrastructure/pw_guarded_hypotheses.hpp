#ifndef PW_GUARDED_HYPOTHESES_HPP
#define PW_GUARDED_HYPOTHESES_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/pw_data_point.hpp"
#include "value_objects/pw_symbols.hpp"

constexpr size_t k_pw_default_max_guards = 2;

// A detector that reads data as a guarded value, `pw(when(c, v), d)`: a default output d, and where the output is
// something else, a condition c and a value v, split outer-inner (`docs/decomposing-detection.md`). Assuming d, every
// point says whether c holds (its output is not d), which is condition data (`input -> 1` or `-> 0`) for the
// condition domain, and the points where it holds are data for the value domain. So the inner function of a sum that
// adds only the primes reads as `pw(when(is_prime(#0), #0), 0)`, and a count over any condition as
// `pw(when(c, 1), 0)`. Each of the `max_guards` most common outputs is tried as d; each condition with each of the
// first `max_guards` values is proposed. Nothing when every output is the same.
template<typename IProposeConditions, typename IProposeValues, typename IMakeLiteral, typename IMakeFunctor>
struct pw_guarded_hypotheses {
    pw_guarded_hypotheses(IProposeConditions& propose_conditions,
                          IProposeValues& propose_values,
                          IMakeLiteral& make_literal,
                          IMakeFunctor& make_functor,
                          pw_symbols symbols,
                          size_t max_guards = k_pw_default_max_guards);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IProposeConditions& propose_conditions_;
    IProposeValues& propose_values_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    pw_symbols symbols_;
    size_t max_guards_;
};

#define PW_GUARDED_HYPOTHESES_TEMPLATE \
    template<typename IProposeConditions, typename IProposeValues, typename IMakeLiteral, typename IMakeFunctor>
#define PW_GUARDED_HYPOTHESES pw_guarded_hypotheses<IProposeConditions, IProposeValues, IMakeLiteral, IMakeFunctor>

PW_GUARDED_HYPOTHESES_TEMPLATE PW_GUARDED_HYPOTHESES::pw_guarded_hypotheses(IProposeConditions& propose_conditions,
                                                                            IProposeValues& propose_values,
                                                                            IMakeLiteral& make_literal,
                                                                            IMakeFunctor& make_functor,
                                                                            pw_symbols symbols,
                                                                            size_t max_guards)
    : propose_conditions_(propose_conditions)
    , propose_values_(propose_values)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols)
    , max_guards_(max_guards) {}

PW_GUARDED_HYPOTHESES_TEMPLATE void PW_GUARDED_HYPOTHESES::propose_hypotheses(const std::vector<pw_data_point>& points,
                                                                             std::vector<const expr*>& out) {
    std::map<const expr*, size_t> counts;
    for (const pw_data_point& point : points)
        ++counts[point.output];
    if (counts.size() < 2)
        return;
    std::vector<std::pair<size_t, const expr*>> common;
    for (const auto& [output, count] : counts)
        common.emplace_back(count, output);
    std::sort(common.rbegin(), common.rend());
    const expr* holds = make_literal_.make_literal(integer(1));
    const expr* fails = make_literal_.make_literal(integer(0));
    for (size_t guard = 0; guard < common.size() && guard < max_guards_; ++guard) {
        const expr* fallback = common[guard].second;
        std::vector<pw_data_point> conditions_data;
        std::vector<pw_data_point> values_data;
        for (const pw_data_point& point : points) {
            const bool guarded = point.output != fallback;
            conditions_data.push_back(pw_data_point{.input = point.input, .output = guarded ? holds : fails});
            if (guarded)
                values_data.push_back(point);
        }
        std::vector<const expr*> conditions;
        propose_conditions_.propose_hypotheses(conditions_data, conditions);
        if (conditions.empty())
            continue;
        std::vector<const expr*> values;
        propose_values_.propose_hypotheses(values_data, values);
        const size_t kept = std::min(values.size(), max_guards_);
        for (const expr* condition : conditions)
            for (size_t index = 0; index < kept; ++index) {
                const expr* value = values[index];
                const expr* region = make_functor_.make_functor(symbols_.region, {condition, value});
                out.push_back(make_functor_.make_functor(symbols_.piecewise, {region, fallback}));
            }
    }
}

#undef PW_GUARDED_HYPOTHESES
#undef PW_GUARDED_HYPOTHESES_TEMPLATE

#endif
