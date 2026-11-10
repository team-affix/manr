#ifndef ARITH_LINEAR_HYPOTHESES_HPP
#define ARITH_LINEAR_HYPOTHESES_HPP

#include <optional>
#include <unordered_set>
#include <vector>
#include "value_objects/arith_symbols.hpp"
#include "value_objects/expr.hpp"
#include "value_objects/integer.hpp"
#include "value_objects/pw_data_point.hpp"

// A detector, not a rewrite rule: it reads the data directly and proposes the line through every pair of points
// with an integer slope, as `a * x + b` with the zero and unit coefficients left out. Search never has to stumble
// into a line one rewrite at a time. Quadratic in the number of points.
template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
struct arith_linear_hypotheses {
    arith_linear_hypotheses(IFindLiteralValue& find_literal_value,
                            IMakeLiteral& make_literal,
                            IMakeFunctor& make_functor,
                            arith_symbols symbols);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    const expr* make_line(const integer& slope, const integer& intercept);

    IFindLiteralValue& find_literal_value_;
    IMakeLiteral& make_literal_;
    IMakeFunctor& make_functor_;
    arith_symbols symbols_;
};

#define ARITH_LINEAR_HYPOTHESES_TEMPLATE template<typename IFindLiteralValue, typename IMakeLiteral, typename IMakeFunctor>
#define ARITH_LINEAR_HYPOTHESES          arith_linear_hypotheses<IFindLiteralValue, IMakeLiteral, IMakeFunctor>

ARITH_LINEAR_HYPOTHESES_TEMPLATE ARITH_LINEAR_HYPOTHESES::arith_linear_hypotheses(
    IFindLiteralValue& find_literal_value, IMakeLiteral& make_literal, IMakeFunctor& make_functor, arith_symbols symbols)
    : find_literal_value_(find_literal_value)
    , make_literal_(make_literal)
    , make_functor_(make_functor)
    , symbols_(symbols) {}

ARITH_LINEAR_HYPOTHESES_TEMPLATE void ARITH_LINEAR_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    std::unordered_set<const expr*> proposed;
    for (size_t first = 0; first < points.size(); ++first) {
        for (size_t second = first + 1; second < points.size(); ++second) {
            const std::optional<integer> x0 = find_literal_value_.find_value(points[first].input);
            const std::optional<integer> y0 = find_literal_value_.find_value(points[first].output);
            const std::optional<integer> x1 = find_literal_value_.find_value(points[second].input);
            const std::optional<integer> y1 = find_literal_value_.find_value(points[second].output);
            if (!x0.has_value() || !y0.has_value() || !x1.has_value() || !y1.has_value())
                continue;
            const integer run = x1.value() - x0.value();
            const integer rise = y1.value() - y0.value();
            if (run == 0 || rise % run != 0)
                continue;
            const integer slope = rise / run;
            const expr* line = make_line(slope, integer(y0.value() - slope * x0.value()));
            const bool fresh = proposed.insert(line).second;
            if (!fresh)
                continue;
            out.push_back(line);
        }
    }
}

ARITH_LINEAR_HYPOTHESES_TEMPLATE const expr* ARITH_LINEAR_HYPOTHESES::make_line(const integer& slope, const integer& intercept) {
    const expr* input = make_functor_.make_functor(symbols_.input, {});
    if (slope == 0)
        return make_literal_.make_literal(intercept);
    const expr* term = slope == 1
        ? input
        : make_functor_.make_functor(symbols_.mul, {make_literal_.make_literal(slope), input});
    if (intercept == 0)
        return term;
    return make_functor_.make_functor(symbols_.add, {term, make_literal_.make_literal(intercept)});
}

#undef ARITH_LINEAR_HYPOTHESES
#undef ARITH_LINEAR_HYPOTHESES_TEMPLATE

#endif
