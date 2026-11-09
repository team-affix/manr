#ifndef ABS_CLOSING_HYPOTHESES_HPP
#define ABS_CLOSING_HYPOTHESES_HPP

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>
#include "value_objects/expr.hpp"
#include "value_objects/pw_data_point.hpp"

// Closes every hypothesis a domain proposes for a piecewise's catch-all over that piecewise's own inputs: a piecewise
// is a function of its keys, so what replaces its catch-all is a closed function, `lam(b)` or `lams(k, b)`, whose body
// b is what the domain proposed (the key's coordinates as `#(k-1)` ... `#0`). k is read from the piecewise's keys
// themselves: a key `point(v1, ..., vk)` has k coordinates, any other key is one input. When every case is explained,
// the piecewise is just that function.
template<typename IProposeHypotheses, typename IMakeFunction>
struct abs_closing_hypotheses {
    abs_closing_hypotheses(IProposeHypotheses& propose_hypotheses, IMakeFunction& make_function, uint32_t point_symbol);
    void propose_hypotheses(const std::vector<pw_data_point>& points, std::vector<const expr*>& out);
private:
    IProposeHypotheses& propose_hypotheses_;
    IMakeFunction& make_function_;
    uint32_t point_symbol_;
    std::vector<const expr*> bodies_;
};

#define ABS_CLOSING_HYPOTHESES_TEMPLATE template<typename IProposeHypotheses, typename IMakeFunction>
#define ABS_CLOSING_HYPOTHESES          abs_closing_hypotheses<IProposeHypotheses, IMakeFunction>

ABS_CLOSING_HYPOTHESES_TEMPLATE ABS_CLOSING_HYPOTHESES::abs_closing_hypotheses(
    IProposeHypotheses& propose_hypotheses, IMakeFunction& make_function, uint32_t point_symbol)
    : propose_hypotheses_(propose_hypotheses)
    , make_function_(make_function)
    , point_symbol_(point_symbol)
    , bodies_() {}

ABS_CLOSING_HYPOTHESES_TEMPLATE void ABS_CLOSING_HYPOTHESES::propose_hypotheses(
    const std::vector<pw_data_point>& points, std::vector<const expr*>& out) {
    if (points.empty())
        return;
    const expr::functor* key = std::get_if<expr::functor>(&points.front().input->content);
    const bool tuple = key != nullptr && key->id == point_symbol_;
    const size_t arity = tuple ? key->args.size() : 1;
    bodies_.clear();
    propose_hypotheses_.propose_hypotheses(points, bodies_);
    for (const expr* body : bodies_)
        out.push_back(make_function_.make_function(arity, body));
}

#undef ABS_CLOSING_HYPOTHESES
#undef ABS_CLOSING_HYPOTHESES_TEMPLATE

#endif
